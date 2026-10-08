#include "FreshSenseNode.h"

// Arranque del dispositivo FreshSense.
// La medicion y el envio estan en FreshSenseNode.h.
// El informe (4.2.7 y 4.2.4) define la lectura y el heartbeat.
// Si el etileno indica HIGH_ETHYLENE, el nodo enciende el LED rojo y la bocina.

FreshSenseNode node;

void setup() {
  node.begin();
}

void loop() {
  node.tick();
}
