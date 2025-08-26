# Simply turning on/off
```C
digitalWrite(RGB_BUILTIN, HIGH);  // Turn the RGB LED white
digitalWrite(RGB_BUILTIN, LOW); // Turn the RGB LED off
```

# Controlling red, green and blue
```C
#define RGB_BRIGHTNESS 64 // defining brightness max 255
rgbLedWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0) // Red LED on with brightness = 64
```