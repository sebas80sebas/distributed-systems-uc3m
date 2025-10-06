from flask import Flask, jsonify
from datetime import datetime

app = Flask(__name__)

@app.route('/fecha', methods=['GET'])
def get_date():
    # Get current date and time in DD/MM/YYYY HH:MM:SS format
    current_date = datetime.now().strftime("%d/%m/%Y %H:%M:%S")
    return jsonify({"fecha": current_date})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
