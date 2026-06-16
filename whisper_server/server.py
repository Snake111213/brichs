"""
Servidor Flask + Whisper para transcripción de audio desde ESP32 Brichs
Recibe audio WAV por POST /transcribe y devuelve el texto transcrito.
Incluye filtro de silencio y parámetros optimizados para comandos de voz.
"""

from flask import Flask, request, render_template_string
import whisper
import numpy as np
import wave
import io
import os

app = Flask(__name__)

# Cargar el modelo de Whisper
print("--- Cargando modelo Whisper (base) ---")
model = whisper.load_model("base")
print("--- Servidor Listo ---")

# Página principal de estado
@app.route('/')
def index():
    return render_template_string('''
        <!DOCTYPE html>
        <html>
        <head>
            <title>Brichs Voice Server</title>
            <style>
                body { font-family: 'Segoe UI', system-ui, sans-serif; background: #0f172a; color: white;
                       display: flex; flex-direction: column; align-items: center; justify-content: center;
                       height: 100vh; margin: 0; }
                .card { background: #1e293b; padding: 2rem; border-radius: 1rem;
                        box-shadow: 0 10px 25px rgba(0,0,0,0.5); text-align: center;
                        border: 1px solid #334155; max-width: 400px; }
                .status { color: #22c55e; font-weight: bold; font-size: 1.2rem; margin-top: 1rem; }
                .dot { height: 10px; width: 10px; background-color: #22c55e; border-radius: 50%;
                       display: inline-block; margin-right: 5px; animation: pulse 2s infinite; }
                @keyframes pulse {
                    0% { transform: scale(0.9); opacity: 0.7; }
                    50% { transform: scale(1.2); opacity: 1; }
                    100% { transform: scale(0.9); opacity: 0.7; }
                }
                .cmds { margin-top: 1.5rem; text-align: left; background: #0f172a;
                         padding: 1rem; border-radius: 0.5rem; font-size: 0.85rem; }
                .cmds li { margin: 4px 0; color: #94a3b8; }
                .cmds strong { color: #06d6a0; }
            </style>
        </head>
        <body>
            <div class="card">
                <h1>🤖 Brichs Voice Server</h1>
                <p>Servidor Whisper para control por voz del robot Brichs.</p>
                <div class="status"><span class="dot"></span> ONLINE</div>
                <div class="cmds">
                    <p style="color:#e2e8f0; font-weight:600;">Comandos reconocidos:</p>
                    <ul>
                        <li><strong>"adelante"</strong> / "avanza" → Avanzar</li>
                        <li><strong>"atrás"</strong> / "retrocede" → Retroceder</li>
                        <li><strong>"izquierda"</strong> → Girar izquierda</li>
                        <li><strong>"derecha"</strong> → Girar derecha</li>
                        <li><strong>"para"</strong> / "stop" → Detener</li>
                    </ul>
                </div>
                <p style="color: #94a3b8; font-size: 0.8rem; margin-top: 1.5rem;">
                    Endpoint de audio: <code>/transcribe</code> (POST)
                </p>
            </div>
        </body>
        </html>
    ''')

@app.route('/transcribe', methods=['POST'])
def transcribe():
    try:
        audio_bytes = request.data
        if not audio_bytes:
            return "No hay audio", 400

        with wave.open(io.BytesIO(audio_bytes), "rb") as wav_file:
            frames = wav_file.readframes(wav_file.getnframes())
            audio_np = np.frombuffer(frames, dtype=np.int16).astype(np.float32) / 32768.0

        # Verificar si hay suficiente energía (filtro de silencio)
        energy = np.mean(np.abs(audio_np))
        if energy < 0.005:
            print("[>] Silencio detectado, ignorando")
            return ""

        # Transcribir con parámetros optimizados para comandos cortos
        result = model.transcribe(
            audio_np,
            language="es",
            no_speech_threshold=0.5,           # Más estricto con el silencio
            logprob_threshold=-0.8,            # Filtrar texto de baja confianza
            condition_on_previous_text=False,   # Evitar alucinaciones en cadena
            temperature=0.0,                    # Menos creatividad = más preciso
        )

        text = result["text"].strip()

        # Filtrar resultados vacíos o muy cortos
        if len(text) <= 1:
            return ""

        print(f"[>] {text}")
        return text
    except Exception as e:
        print(f"[!] Error: {str(e)}")
        return f"Error: {str(e)}", 500

if __name__ == '__main__':
    print("\\n=== Brichs Voice Server ===")
    print("Escuchando en http://0.0.0.0:5000")
    print("Endpoint: POST /transcribe\\n")
    app.run(host='0.0.0.0', port=5000)
