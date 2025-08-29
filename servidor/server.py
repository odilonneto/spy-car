from flask import Flask, request, send_from_directory, render_template_string
from flask_socketio import SocketIO
import os
import base64
import eventlet

eventlet.monkey_patch()

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = 'uploads'
os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)

socketio = SocketIO(app, cors_allowed_origins='*')

@app.route('/')
def index():
    return render_template_string("""
    <!doctype html>
    <html lang="pt-br">
      <head>
        <meta charset="utf-8">
        <title>SpyCam</title>
        <style>
          body { margin: 0; background: black; display: flex; justify-content: center; align-items: center; height: 100vh; }
          img { display: block; max-width: 100%; height: auto; }
        </style>
      </head>
      <body>
        <img id="cam" src="">
        <script src="https://cdn.socket.io/4.6.1/socket.io.min.js"></script>
        <script>
          const socket = io();
          socket.on('new_frame', data => {
            document.getElementById('cam').src = 'data:image/jpeg;base64,' + data;
          });
        </script>
      </body>
    </html>
    """)

@app.route('/upload', methods=['POST'])
def upload():
    try:
        # Salva imagem recebida
        img_path = os.path.join(app.config['UPLOAD_FOLDER'], 'frame.jpg')
        with open(img_path, 'wb') as f:
            f.write(request.data)

        # Codifica imagem para base64
        encoded = base64.b64encode(request.data).decode('utf-8')

        # Envia imagem via WebSocket
        socketio.emit('new_frame', encoded)
        return 'OK', 200
    except Exception as e:
        return f'Erro: {e}', 500

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000)
