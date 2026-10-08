#include "FreshSenseNode.h"

// Arranque del dispositivo FreshSense.
// La medicion y el envio estan en FreshSenseNode.h.
// El informe (4.2.7 y 4.2.4) define esas dos tareas:
// publicar la lectura y publicar el heartbeat.

FreshSenseNode node;

void setup() {
  node.begin();
}

void loop() {
  node.tick();
}
