
/*
 * Adaptado para ligar e desligar um LED usando Arduino Mega e Ethernet Shield e node-red.
 * O broker utilizado MQTT  HIVEMQ instalado no computador
 * Tutorial canal youtube. https://www.youtube.com/watch?v=Iuj7BvXbYgY&list=PLDkRfSMHz1RiTlzfSbeZZcYf80G0sYl3m
 * Eng.Carlos kwiek.       https://github.com/ckwiek/Tutorial_Node_RED
 * contato@carloskwiek.com.br 
 */

 
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

 
const int ledPin = 13;       // Conecte um LED entre o GND e o port 13.

// Update these with values suitable for your network.
byte mac[]    = {0x90, 0xA2, 0xDA, 0x0F, 0x82, 0xD0};

IPAddress ip(192, 168, 0, 30); // Aqui digite um IP ou mantenha este mesmo se for compativel com sua rede local

IPAddress server(192, 168, 0, 6);// Aqui digite o IP do seu broker

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
   
  
    if ((char)payload[0] == 'L'){
    digitalWrite (ledPin, HIGH);
    }
    else{
      digitalWrite(ledPin, LOW);
   
      }
     }

EthernetClient ethClient;
PubSubClient client(ethClient);

    void reconnect() {
    // Loop until were reconnected
    while (!client.connected()) {
      Serial.print("Aguardando conectar...");
      // Attempt to connect
      if (client.connect("ESP8266Client")) {
        Serial.println("conectado!");
         //Once connected, publish an announcement...
         client.publish("liga", "led"); // No node-red crie o topico led
         //... and resubscribe
        client.subscribe("led");       
    
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
  }

void setup()
{
  Serial.begin(57600);

  client.setServer(server, 1883);
  client.setCallback(callback);

  Ethernet.begin(mac, ip);
  // Allow the hardware to sort itself out
  delay(5000);

   // initialize the LED as an output:
   pinMode(ledPin, OUTPUT);
    
}

void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
