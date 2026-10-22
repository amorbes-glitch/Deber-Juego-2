# LA AVENTURA DE LA GOTA LUNITA

Juego educativo interactivo en C++ para Windows. Acompaña a Lunita por tres mundos con diez desafíos sobre los estados del agua y el ciclo del agua. Incluye dibujos animados en pantalla, respuestas tocando o arrastrando, botones de calor y frío, sonido sintetizado, estrellas y una pantalla de victoria con arcoíris. Cada respuesta correcta avanza automáticamente al siguiente desafío y, al terminar un mundo, comienza el siguiente nivel.

## Compilar

Se necesita MinGW-w64 con el compilador `g++`:

```powershell
g++ -std=c++17 -O2 -municode -mwindows main.cpp -o Lunita.exe -lgdi32 -luser32 -lwinmm
```

Después, inicia el juego con:

```powershell
.\Lunita.exe
```

No requiere imágenes, paquetes, conexión a Internet ni archivos de sonido externos.

## Aprendizajes

- **Mundo 1:** agua líquida, hielo sólido y vapor de agua invisible.
- **Mundo 2:** derretimiento, congelación, evaporación y condensación.
- **Mundo 3:** el Sol calienta el agua, el vapor que se enfría forma gotitas o cristales diminutos en las nubes, y la lluvia devuelve agua a la superficie.

Las nubes visibles contienen pequeñas gotas de agua líquida o cristales de hielo: no son vapor visible.
