from enum import Enum
import argparse
import socket
import threading
import os
import sys
import time

class client:
    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum):
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _listener_socket = None
    _listener_thread = None
    _connected_user = None
    _registered_user = None
    _listening_port = None
    _running = True

    # ******************** METHODS *******************
    # Function to establish connection with the server
    @staticmethod
    def connect_to_server():
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect((client._server, client._port))
            return sock
        except Exception as e:
            print(f"Error connecting to server: {e}")
            return None

    # Function to send strings through the socket
    @staticmethod
    def send_string(sock, string):
        sock.sendall(string.encode() + b'\0')

    # Function to receive strings from the socket
    @staticmethod
    def receive_string(sock):
        data = bytearray()
        while True:
            byte = sock.recv(1)
            if not byte or byte == b'\0':
                break
            data.extend(byte)
        return data.decode()

    # Function to find a free port and create listener socket
    @staticmethod
    def create_listener_socket():
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(('0.0.0.0', 0))  # Random port
        sock.listen(5)
        return sock, sock.getsockname()[1]

    # Function that handles file transfers
    @staticmethod
    def handle_file_transfer(client_socket, client_address):
        try:
            # Receive command
            operation = client.receive_string(client_socket)
            
            if operation == "GET_FILE":
                # Receive file name
                file_name = client.receive_string(client_socket)
                
                # Check if file exists
                if not os.path.isfile(file_name):
                    client_socket.send(b'\x01')  # Code 1: File does not exist
                    return
                
                # Send success code
                client_socket.send(b'\x00')  # Code 0: Success
                
                # Get file size
                file_size = os.path.getsize(file_name)
                client.send_string(client_socket, str(file_size))
                
                # Send file content
                with open(file_name, 'rb') as f:
                    data = f.read(4096)
                    while data:
                        client_socket.send(data)
                        data = f.read(4096)
        except Exception as e:
            print(f"Error in file transfer: {e}")
        finally:
            client_socket.close()

    # Function for the listener thread
    @staticmethod
    def listener_thread():
        while client._running:
            try:
                client_socket, client_address = client._listener_socket.accept()
                # Create a thread to handle file transfer
                transfer_thread = threading.Thread(
                    target=client.handle_file_transfer,
                    args=(client_socket, client_address)
                )
                transfer_thread.daemon = True
                transfer_thread.start()
            except Exception as e:
                if client._running:
                    print(f"Error in listener thread: {e}")
                break

    # Register Method
    @staticmethod
    def register(user):
        sock = client.connect_to_server()
        if not sock:
            print("c > REGISTER FAIL")
            return client.RC.ERROR
        
        try:
            # Send operation
            client.send_string(sock, "REGISTER")
            # Send username
            client.send_string(sock, user)
            
            # Receive response
            response = sock.recv(1)[0]
            
            if response == 0:
                print("c > REGISTER OK")
                client._registered_user = user
                return client.RC.OK
            elif response == 1:
                print("c > USERNAME IN USE")
                return client.RC.USER_ERROR
            else:
                print("c > REGISTER FAIL")
                return client.RC.ERROR
        except Exception as e:
            print(f"c > REGISTER FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # Unregister Method
    @staticmethod
    def unregister(user):
        sock = client.connect_to_server()
        if not sock:
            client._registered_user = None
            print("c > UNREGISTER FAIL")
            return client.RC.ERROR
        
        try:
            # Disconnect if connected with this user
            if client._connected_user == user:
                client.disconnect(user)
            
            # Send operation
            client.send_string(sock, "UNREGISTER")
            # Send username
            client.send_string(sock, user)
            
            # Receive response
            response = sock.recv(1)[0]
            
            if response == 0:
                client._registered_user = None
                print("c > UNREGISTER OK")
                return client.RC.OK
            elif response == 1:
                client._registered_user = None
                print("c > USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                client._registered_user = None
                print("c > UNREGISTER FAIL")
                return client.RC.ERROR
        except Exception as e:
            client._registered_user = None
            print("c > UNREGISTER FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # Connect Method
    @staticmethod
    def connect(user):
                
        # Create listener socket if it doesn't exist
        if not client._listener_socket:
            try:
                client._listener_socket, client._listening_port = client.create_listener_socket()
                # Create listener thread
                client._running = True
                client._listener_thread = threading.Thread(target=client.listener_thread)
                client._listener_thread.daemon = True
                client._listener_thread.start()
            except Exception as e:
                print(f"c > CONNECT FAIL")
                return client.RC.ERROR
        
        sock = client.connect_to_server()
        if not sock:
            print("c > CONNECT FAIL")
            return client.RC.ERROR
        
        try:

            # Send operation
            client.send_string(sock, "CONNECT")
            # Send username
            client.send_string(sock, user)
            # Send listening port
            client.send_string(sock, str(client._listening_port))
            
            # Receive response
            response = sock.recv(1)[0]
            
            if response == 0:
                client._connected_user = user
                print("c > CONNECT OK")
                return client.RC.OK
            elif response == 1:
                print("c > CONNECT FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif response == 2:
                print("c > USER ALREADY CONNECTED")
                return client.RC.USER_ERROR
            else:
                print("c > CONNECT FAIL")
                return client.RC.ERROR
        except Exception as e:
            print("c > CONNECT FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # Disconnect Method
    @staticmethod
    def disconnect(user):
        
        sock = client.connect_to_server()
        if not sock:
            # Even if communication with server fails, close listener socket
            client._running = False
            if client._listener_socket:
                client._listener_socket.close()
                client._listener_socket = None
            if client._listener_thread:
                client._listener_thread.join(1)
                client._listener_thread = None
            client._connected_user = None
            print("c > DISCONNECT FAIL")
            return client.RC.ERROR
        
        try:
            # Send operation
            client.send_string(sock, "DISCONNECT")
            # Send username
            client.send_string(sock, user)
            
            # Receive response
            response = sock.recv(1)[0]
            
            # Always close listener socket
            client._running = False
            if client._listener_socket:
                client._listener_socket.close()
                client._listener_socket = None
            if client._listener_thread:
                client._listener_thread.join(1)
                client._listener_thread = None
            
            if response == 0:
                client._connected_user = None
                print("c > DISCONNECT OK")
                return client.RC.OK
            elif response == 1:
                client._connected_user = None
                print("c > DISCONNECT FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif response == 2:
                client._connected_user = None
                print("c > DISCONNECT FAIL, USER NOT CONNECTED")
                return client.RC.USER_ERROR
            else:
                client._connected_user = None
                print("c > DISCONNECT FAIL")
                return client.RC.ERROR
        except Exception as e:
            # Even if it fails, close listener socket
            client._running = False
            if client._listener_socket:
                client._listener_socket.close()
                client._listener_socket = None
            if client._listener_thread:
                client._listener_thread.join(1)
                client._listener_thread = None
            client._connected_user = None
            print("c > DISCONNECT FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # Publish Method
    @staticmethod
    def publish(fileName, description):
                
        sock = client.connect_to_server()
        if not sock:
            print("c > PUBLISH FAIL")
            return client.RC.ERROR
        
        try:
            # Send operation
            client.send_string(sock, "PUBLISH")
            # Send username (can be None if not registered)
            if not client._registered_user:
                client.send_string(sock, "__NONE__")  # Send empty string if no user
            else:
                client.send_string(sock, client._registered_user)
            # Send filename
            client.send_string(sock, fileName)
            # Send description
            client.send_string(sock, description)
            
            # Receive response
            response = sock.recv(1)[0]
	    
            if response == 0:
                print("c > PUBLISH OK")
                # If file doesn't exist, create it using description as content
                if not os.path.isfile(fileName):
                    with open(fileName, 'w') as f:
                        f.write(description + '\n')
                return client.RC.OK
            elif response == 1:
                print("c > PUBLISH FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif response == 2:
                print("c > PUBLISH FAIL, USER NOT CONNECTED")
                return client.RC.USER_ERROR
            elif response == 3:
                print("c > PUBLISH FAIL, CONTENT ALREADY PUBLISHED")
                return client.RC.USER_ERROR
                
            else:
                print("c > PUBLISH FAIL")
                return client.RC.ERROR
        except Exception as e:
        
            print("c > PUBLISH FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # Delete Method
    @staticmethod
    def delete(fileName):
                
        sock = client.connect_to_server()
        if not sock:
            print("c > DELETE FAIL")
            return client.RC.ERROR
        
        try:
            # Send operation
            client.send_string(sock, "DELETE")
            # Send username (can be None if not registered)
            if not client._registered_user:
                client.send_string(sock, "__NONE__")  # Send empty string if no user
            else:
                client.send_string(sock, client._registered_user)
            # Send filename
            client.send_string(sock, fileName)
            
            # Receive response
            response = sock.recv(1)[0]
            
            if response == 0:
                print("c > DELETE OK")
                # Delete local file if it exists
                if os.path.exists(fileName):
                    os.remove(fileName)
                return client.RC.OK
            elif response == 1:
                print("c > DELETE FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif response == 2:
                print("c > DELETE FAIL, USER NOT CONNECTED")
                return client.RC.USER_ERROR
            elif response == 3:
                print("c > DELETE FAIL, CONTENT NOT PUBLISHED")
                return client.RC.USER_ERROR
            else:
                print("c > DELETE FAIL")
                return client.RC.ERROR
        except Exception as e:
            print("c > DELETE FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # ListUsers Method
    @staticmethod
    def listusers():
                
        sock = client.connect_to_server()
        if not sock:
            print("c > LIST_USERS FAIL")
            return client.RC.ERROR
        
        try:
            # Send operation
            client.send_string(sock, "LIST_USERS")
            # Send username (can be None if not registered)
            if not client._registered_user:
                client.send_string(sock, "__NONE__")  # Send empty string if no user
            else:
                client.send_string(sock, client._registered_user)
            
            # Receive response
            response = sock.recv(1)[0]
            
            if response == 0:
                # Receive number of users
                num_users_str = client.receive_string(sock)
                num_users = int(num_users_str)
                
                print("c > LIST_USERS OK")
                # Receive information for each user
                for _ in range(num_users):
                    username = client.receive_string(sock)
                    ip = client.receive_string(sock)
                    port = client.receive_string(sock)
                    print(f"{username} {ip} {port}")
                
                return client.RC.OK
            elif response == 1:
                print("c > LIST_USERS FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif response == 2:
                print("c > LIST_USERS FAIL, USER NOT CONNECTED")
                return client.RC.USER_ERROR
            else:
                print("c > LIST_USERS FAIL")
                return client.RC.ERROR
        except Exception as e:
            print("c > LIST_USERS FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # ListContent Method
    @staticmethod
    def listcontent(user):
                
        sock = client.connect_to_server()
        if not sock:
            print("c > LIST_CONTENT FAIL")
            return client.RC.ERROR
        
        try:
            # Send operation
            client.send_string(sock, "LIST_CONTENT")
           # Send username (can be None if not registered)
            if not client._registered_user:
                client.send_string(sock, "__NONE__")  # Send empty string if no user
            else:
                client.send_string(sock, client._registered_user)
            # Send remote username
            client.send_string(sock, user)
            
            # Receive response
            response = sock.recv(1)[0]
            
            if response == 0:
                # Receive number of files
                num_files_str = client.receive_string(sock)
                num_files = int(num_files_str)
                
                print("c > LIST_CONTENT OK")
                # Receive name of each file
                for _ in range(num_files):
                    filename = client.receive_string(sock)
                    print(f"{filename}")
                
                return client.RC.OK
            elif response == 1:
                print("c > LIST_CONTENT FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif response == 2:
                print("c > LIST_CONTENT FAIL, USER NOT CONNECTED")
                return client.RC.USER_ERROR
            elif response == 3:
                print("c > LIST_CONTENT FAIL, REMOTE USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                print("c > LIST_CONTENT FAIL")
                return client.RC.ERROR
        except Exception as e:
            print("c > LIST_CONTENT FAIL")
            return client.RC.ERROR
        finally:
            sock.close()

    # GetFile Method
    @staticmethod
    def getfile(user, remote_FileName, local_FileName):
                
        # Get remote user information
        sock = client.connect_to_server()
        if not sock:
            print("c > GET_FILE FAIL")
            return client.RC.ERROR
        
        try:
            # Send operation to list users
            client.send_string(sock, "LIST_USERS")
            # Send username
            client.send_string(sock, client._connected_user)
            
            # Receive response
            response = sock.recv(1)[0]
            
            if response != 0:
                print("c > GET_FILE FAIL")
                return client.RC.ERROR
            
            # Receive number of users
            num_users_str = client.receive_string(sock)
            num_users = int(num_users_str)
            
            # Search for remote user
            remote_ip = None
            remote_port = None
            for _ in range(num_users):
                username = client.receive_string(sock)
                ip = client.receive_string(sock)
                port = client.receive_string(sock)
                
                
                if username == user:
                    remote_ip = ip
                    remote_port = int(port)
                    break
        except Exception as e:
            print("c > GET_FILE FAIL")
            return client.RC.ERROR
        finally:
            sock.close()
        
        # Check if we found the remote user
        if not remote_ip or not remote_port:
            print("c > GET_FILE FAIL")
            return client.RC.ERROR
        
        # Connect with remote client to request the file
        try:
            remote_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            remote_sock.connect((remote_ip, remote_port))
            
            # Send operation
            client.send_string(remote_sock, "GET_FILE")
            # Send remote filename
            client.send_string(remote_sock, remote_FileName)
            
            # Receive response
            response = remote_sock.recv(1)[0]
            
            if response == 0:
                # Receive file size
                file_size_str = client.receive_string(remote_sock)
                file_size = int(file_size_str)
                
                # Create local file
                try:
                    with open(local_FileName, 'wb') as f:
                        bytes_received = 0
                        while bytes_received < file_size:
                            # Receive data
                            data = remote_sock.recv(min(4096, file_size - bytes_received))
                            if not data:
                                break
                            
                            # Write data to file
                            f.write(data)
                            bytes_received += len(data)
                    
                    if bytes_received == file_size:
                        print("c > GET_FILE OK")
                        return client.RC.OK
                    else:
                        # Transfer not completed, delete the file
                        os.remove(local_FileName)
                        print("c > GET_FILE FAIL")
                        return client.RC.ERROR
                except Exception as e:
                    # Error writing file, delete it if it exists
                    if os.path.exists(local_FileName):
                        os.remove(local_FileName)
                    print("c > GET_FILE FAIL")
                    return client.RC.ERROR
            elif response == 1:
                print("c > GET_FILE FAIL, FILE NOT EXIST")
                return client.RC.USER_ERROR
            else:
                print("c > GET_FILE FAIL")
                return client.RC.ERROR
        except Exception as e:
            print("c > GET_FILE FAIL")
            return client.RC.ERROR
        finally:
            remote_sock.close()

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():
        while (True):
            try:
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):
                    line[0] = line[0].upper()

                    if (line[0] == "REGISTER"):
                        if (len(line) == 2):
                            client.register(line[1])
                        else:
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif (line[0] == "UNREGISTER"):
                        if (len(line) == 2):
                            client.unregister(line[1])
                        else:
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif (line[0] == "CONNECT"):
                        if (len(line) == 2):
                            client.connect(line[1])
                        else:
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif (line[0] == "PUBLISH"):
                        if (len(line) >= 3):
                            # Remove first two words
                            description = ' '.join(line[2:])
                            client.publish(line[1], description)
                        else:
                            print("Syntax error. Usage: PUBLISH <fileName> <description>")

                    elif (line[0] == "DELETE"):
                        if (len(line) == 2):
                            client.delete(line[1])
                        else:
                            print("Syntax error. Usage: DELETE <fileName>")

                    elif (line[0] == "LIST_USERS"):
                        if (len(line) == 1):
                            client.listusers()
                        else:
                            print("Syntax error. Use: LIST_USERS")

                    elif (line[0] == "LIST_CONTENT"):
                        if (len(line) == 2):
                            client.listcontent(line[1])
                        else:
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif (line[0] == "DISCONNECT"):
                        if (len(line) == 2):
                            client.disconnect(line[1])
                        else:
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif (line[0] == "GET_FILE"):
                        if (len(line) == 4):
                            client.getfile(line[1], line[2], line[3])
                        else:
                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif (line[0] == "QUIT"):
                        if (len(line) == 1):
                            # If there's a connected user, disconnect before exiting
                            if client._connected_user:
                                client.disconnect(client._connected_user)
                            break
                        else:
                            print("Syntax error. Use: QUIT")
                    else:
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage():
        print("Usage: python3 client.py -s <server> -p <port>")

    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def parseArguments(argv):
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535")
            return False

        client._server = args.s
        client._port = args.p

        return True

    # ******************** MAIN *********************
    @staticmethod
    def main(argv):
        if (not client.parseArguments(argv)):
            client.usage()
            return

        client.shell()
        print("+++ FINISHED +++")

if __name__ == "__main__":
    client.main(sys.argv)
