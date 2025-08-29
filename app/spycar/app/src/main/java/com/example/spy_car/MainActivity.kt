package com.example.spy_car

import android.annotation.SuppressLint
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.DatabaseReference
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener

class MainActivity : AppCompatActivity() {
    private lateinit var webViewStream: WebView
    private lateinit var btnLeft: Button
    private lateinit var btnRight: Button
    private lateinit var btnUp: Button
    private lateinit var btnDown: Button
    private lateinit var btnLogout: Button
    private lateinit var btnLed: Button
    private var ledOn = false
    private lateinit var joystick: SimpleJoystickView
    private lateinit var databaseReference: DatabaseReference
    private lateinit var auth: FirebaseAuth


    @SuppressLint("MissingInflatedId")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        auth = FirebaseAuth.getInstance()
        val currentUser = auth.currentUser
        if (currentUser == null) {
            // Se não estiver autenticado, redireciona para a tela de login
            val intent = Intent(this, AuthActivity::class.java)
            startActivity(intent)
            finish()
            return
        }

        databaseReference = FirebaseDatabase.getInstance().getReference("livestream")
        databaseReference.child("url").addValueEventListener(object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                val url = snapshot.value?.toString()
                if (!url.isNullOrBlank()) {
                    webViewStream.loadUrl(url)
                }
            }
            override fun onCancelled(error: DatabaseError) {
                Toast.makeText(this@MainActivity, "Erro ao escutar URL de transmissão: ${error.message}", Toast.LENGTH_SHORT).show()
            }
        })


        // WebView
        webViewStream = findViewById(R.id.webViewStream)
        val ws = webViewStream.settings
        ws.javaScriptEnabled = true

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            ws.mixedContentMode = WebSettings.MIXED_CONTENT_ALWAYS_ALLOW
        }

        ws.domStorageEnabled = true
        ws.cacheMode = WebSettings.LOAD_DEFAULT
        webViewStream.webViewClient = WebViewClient()

        databaseReference = FirebaseDatabase.getInstance().getReference("livestream")
        databaseReference.child("url").get()
            .addOnSuccessListener { snapshot ->
                val url = snapshot.value.toString()
                webViewStream.loadUrl(url) // aqui está certo
            }
            .addOnFailureListener {
                Toast.makeText(this, "Erro ao recuperar a URL de transmissão", Toast.LENGTH_SHORT).show()
            }


        btnLeft = findViewById(R.id.btnLeft)
        btnRight = findViewById(R.id.btnRight)
        btnUp = findViewById(R.id.btnUp)
        btnDown = findViewById(R.id.btnDown)
        btnLogout = findViewById(R.id.btnLogout)
        btnLed = findViewById(R.id.btnLed)
        joystick = findViewById(R.id.joystickView)

        databaseReference = FirebaseDatabase.getInstance().getReference("comandos")

        btnLeft.setOnClickListener { sendServo("left") }
        btnRight.setOnClickListener { sendServo("right") }
        btnUp.setOnClickListener { sendServo("up") }
        btnDown.setOnClickListener { sendServo("down") }
        btnLed.setOnClickListener {
            ledOn = !ledOn
            val command = if (ledOn) "on" else "off"
            btnLed.text = if (ledOn) "Off" else "On"
            sendLed(command)
        }
        btnLogout.setOnClickListener {
            auth.signOut()
            val intent = Intent(this, AuthActivity::class.java)
            startActivity(intent)
            finish()
        }

        joystick.setOnMoveListener { angle, strength ->
            val level = when {
                strength < 10f -> 0
                strength < 40f -> 0.7
                strength < 70f -> 0.85
                else -> 1.0
            }

            val command = when {
                level == 0 -> "stop"
                angle in 45f..135f -> "forward:$level"
                angle in 135f..225f -> "left:$level"
                angle in 225f..315f -> "backward:$level"
                else -> "right:$level"
            }

            sendMotor(command)
        }
    }

    private fun sendServo(command: String) {
        val fullCommand = "$command:${System.currentTimeMillis()}"
        databaseReference.child("servo").setValue(fullCommand)
    }

    private fun sendMotor(command: String) {
        databaseReference.child("motor").setValue(command)
    }

    private fun sendLed(command: String) {
        val fullCommand = "$command:${System.currentTimeMillis()}"
        databaseReference.child("led").setValue(fullCommand)
    }

    override fun onStart() {
        super.onStart()
        val currentUser = FirebaseAuth.getInstance().currentUser
        if (currentUser == null) {
            val intent = Intent(this, AuthActivity::class.java)
            startActivity(intent)
            finish()
        }
    }
}