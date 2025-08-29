package com.example.spy_car

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.min

class SimpleJoystickView(context: Context, attrs: AttributeSet) : View(context, attrs) {

    private val basePaint = Paint().apply {
        style = Paint.Style.FILL
        alpha = 100
    }
    private val hatPaint = Paint().apply {
        style = Paint.Style.FILL
        alpha = 200
    }

    private var centerX = 0f
    private var centerY = 0f
    private var baseRadius = 0f
    private var hatRadius = 0f
    private var touchX = 0f
    private var touchY = 0f

    private var moveListener: ((angle: Float, strength: Float) -> Unit)? = null

    fun setOnMoveListener(listener: (Float, Float) -> Unit) {
        moveListener = listener
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        centerX = (w / 2).toFloat()
        centerY = (h / 2).toFloat()
        baseRadius = min(w, h).toFloat() / 3f
        hatRadius = min(w, h).toFloat() / 6f
        touchX = centerX
        touchY = centerY
    }

    override fun onDraw(canvas: Canvas) {
        // Base
        basePaint.color = 0xFF888888.toInt()
        canvas.drawCircle(centerX, centerY, baseRadius, basePaint)
        // Hat
        hatPaint.color = 0xFF5555FF.toInt()
        canvas.drawCircle(touchX, touchY, hatRadius, hatPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val x = event.x
        val y = event.y

        var dx = x - centerX
        var dy = y - centerY

        if (kotlin.math.abs(dx) > kotlin.math.abs(dy)) {
            dy = 0f
        } else {
            dx = 0f
        }

        val dist = hypot(dx.toDouble(), dy.toDouble()).toFloat()


        when (event.action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                // Limita o “hat” dentro da base
                if (dist < baseRadius) {
                    touchX = centerX + dx
                    touchY = centerY + dy
                } else {
                    val ratio = baseRadius / dist
                    touchX = centerX + dx * ratio
                    touchY = centerY + dy * ratio
                }
                // Ângulo em graus
                val angleDeg = (Math.toDegrees(
                    atan2((centerY - touchY).toDouble(), (touchX - centerX).toDouble())
                ) + 360.0) % 360.0
                val angleF = angleDeg.toFloat()
                // Força 0–100
                val strength = (dist / baseRadius * 100).coerceAtMost(100f)
                moveListener?.invoke(angleF, strength)
            }
            MotionEvent.ACTION_UP -> {
                touchX = centerX
                touchY = centerY
                moveListener?.invoke(0f, 0f)
            }
        }
        invalidate()
        return true
    }
}
