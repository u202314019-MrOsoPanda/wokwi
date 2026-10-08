#include "FreshSenseNode.h"

// Arranque del dispositivo FreshSense.
// sketch.ino solo crea el nodo. La medicion, los actuadores y el envio
// estan explicados clase por clase en FreshSenseNode.h.
// HIGH_ETHYLENE: LED rojo y bocina.
// TEMP_RISK: rele del ventilador. En la placa fisica el motor es un ventilador.

FreshSenseNode node;

void setup() {
  node.begin();
}

void loop() {
  node.tick();
}
