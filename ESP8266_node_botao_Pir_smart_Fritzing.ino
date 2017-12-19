/*
 * Bibliotecas utilizadas:
 * 
 * PubSubClient: https://github.com/knolleary/pubsubclient
 * Adafruit DHT22/DHT11 library: https://github.com/adafruit/DHT-sensor-library
 *                               Seguir instruções de: https://learn.adafruit.com/dht?view=all 
 */
 
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

//Defines do MQTT
#define ID_MQTT     "MQTTCarlos"

//Defines do ADC e do envio de sua leitura
#define CANAL_ADC          0     //Aqui, define-se em qual canal ADC está ligado o potenciômetro (e, consequentemente, qual ADC será lido).
                                 //No ESP32, este pode ser 0 ou 1. No NodeMCU, pode ser somente 0. 
#define TEMPO_ENVIO_ADC    1000  //tempo (em milisegundos) em que será enviado o valor de ADC via MQTT                       

//defines do sensor de temperatura e umidade DHT11
#define TEMPO_ENVIO_TEMP_UMIDADE  2000 //tempo (em milisegundos) em que será enviado o valor de temperatura e umidade via MQTT 
                                       //Para o DHT11, segundo a documentação (https://learn.adafruit.com/dht?view=all), o tempo mínimo entre
                                       //duas leituras consecutivas deve ser 2 segundos.                         
#define DHTPIN                    D4   //GPIO do NodeMCU em que o sensor está ligado
#define DHTTYPE DHT11                  //Descomente esta linha para usar o sensor DHT 11
//#define DHTTYPE DHT22                //Descomente esta linha para usar o sensor DHT 22  (AM2302), AM2321
//#define DHTTYPE DHT21                //Descomente esta linha para usar o sensor DHT 21 (AM2301) 


//defines do sensor PIR
#define ESTADO_PIR_AGUARDA_MOVIMENTO               0      //estado da máquina de estados do sensor PIR
#define ESTADO_PIR_AGUARDA_CONFIRMACAO_MOVIMENTO   1      //estado da máquina de estados do sensor PIR
#define ESTADO_PIR_AGUARDA_FIM_DE_MOVIMENTO        2      //estado da máquina de estados do sensor PIR
#define TEMPO_CONFIRMACAO_MOVIMENTO                500   //tempo, em milisegundos, para confirmação de movimento detectado pelo sensor PIR
  
  
//constantes relacionadas ao MQTT
const char* ssid = "REDE";                       //coloque aqui o nome da sua rede
const char* password = "SENHA";              //coloque aqui sua senha
const char* mqtt_server = "192.168.0.6";          // coloque aqui o IP/endereço do broker
//const char* mqtt_server = "iot.eclipse.org";   //utilizado para testes / validação

//constantes relacionadas a entradas e saidas
const int buttonPin = D1;    //Pino no qual estará ligado o botão
const int ledPin = D7;       //Pino no qual estará ligado o LED
const int PIRPin = D5;       //Pino no qual estara ligado o sensor PIR 
const int smart =  D6;

//variáveis globais de temporização
unsigned long TemporizacaoADC;
unsigned long TemporizacaoTempUmid;
unsigned long TemporizacaoSensorPIR;

//variáveis e objetos globais   
WiFiClient espClient;
PubSubClient client(espClient);
char EstadoLED;
DHT dht(DHTPIN, DHTTYPE);
char EstadoMaquinaSensorPIR;
 
//prototypes
void connecta_wifi(void);
void callback(char* topic, byte* payload, unsigned int length);
void reconecta_mqtt(void);
void reconecta_wifi(void);
void verifica_botao(void);
unsigned long tempo_decorrido(unsigned long tempo);
void verifica_e_envia_valor_potenciometro(void);
void verifica_e_envia_temperatura_e_umidade(void);
void maquina_estados_sensor_PIR(void);

/*
*   IMPLEMENTAÇÕES
*/

//Função: Conecta-se ao WI-FI
//Parâmetros: nenhum
//Retorno: nenhum
void connecta_wifi(void) 
{   
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Conectando-se a ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
   
    //aguarda conexao
    while (WiFi.status() != WL_CONNECTED) 
    {
      delay(500);
      Serial.print(".");
    }
   
    Serial.println("");
    Serial.println("Conectado ao WI-FI!!");
    Serial.println("IP: ");
    Serial.println(WiFi.localIP());
} 
 
//Função: Conecta-se ao WI-FI
//Parâmetros: nenhum
//Retorno: nenhum
void callback(char* topic, byte* payload, unsigned int length) 
{
    Serial.print("Chegou uma mensagem no topico");
    Serial.print(topic);
    Serial.print(": ");
  
    for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
   
    //interpreta comando recebido   
    if ((char)payload[0] == 'D') 
    {
        digitalWrite(ledPin, HIGH);    
        EstadoLED = 1;
    }
    if ((char)payload[0] == 'L') 
    {
        digitalWrite(ledPin, LOW);
        EstadoLED = 0;
    }
} 

//Função: reconecta-se ao broker MQTT
//Parâmetros: nenhum
//Retorno: nenhum
void reconecta_mqtt(void) {    
    while (!client.connected()) 
  {
        Serial.print("Tentando se reconectar no broker MQTT...");
      
        if (client.connect(ID_MQTT)) {
            //envia mensagem para o tópico de publish avisando que se conectou e da subscribe no tópico desejado  
            Serial.println("conectado!");
            client.publish("Liga", "Leds"); 
            client.subscribe("Leds");
        } 
      else 
      {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" tentando novamente em 2 segundos");        
            delay(2000);
       } 
    }
}

//Função: reconecta-se ao WiFi
//Parâmetros: nenhum
//Retorno: nenhum
void reconecta_wifi(void) 
{
    //se já está conectado a rede WI-FI, nada é feito. 
    //Caso contrário, são efetuadas novas tentativas de conexão
    if (WiFi.status() == WL_CONNECTED)
        return;
        
     WiFi.begin(ssid, password); // Conecta na rede WI-FI
    
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        Serial.print(".");
        ESP.wdtFeed();
    }
  
    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.print(ssid);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
}


//Função: verifica acionamento do botão
//Parametros: nenhum
//Retorno: nenhum
void verifica_botao(void)
{
    if (!digitalRead(buttonPin))
        delay(20); //20ms de tempo para debounce
    else
        return;  
  
    if (!digitalRead(buttonPin))
    {
        //acionamento do botão confirmado.     
        if (EstadoLED == 1) 
        {
            digitalWrite(ledPin, LOW); 
            EstadoLED = 0;
            client.publish("Botao", "Fisico"); 
        }
        else    
        {
          digitalWrite(ledPin, HIGH); 
          EstadoLED = 1;  
          client.publish("Botao", "Fisico."); 
        }    

        //aguarda o botao ser solto
        while (digitalRead(buttonPin) == 0)
        {
            ESP.wdtFeed();
        }
    }    
}

//Função: retorna o tempo decorrido (em milisegundos) desde o tempo passado como parâmetro
//Parametros: tempo a ser considerado
//Retorno: tempo decorrido desde o tempo passado como parâmetro
unsigned long tempo_decorrido(unsigned long tempo)
{
  unsigned long TempoDecorridoCalculado;

  TempoDecorridoCalculado = millis() - tempo;
  return TempoDecorridoCalculado;
}

//Função: verifica se deve enviar via MQTT o valor do potenciômetro. Se sim, faz o envio.
//Parametros: nenhum
//Retorno: nenhum
void verifica_e_envia_valor_potenciometro(void)
{
  long ValorLidoADC;          //O ADC é de 12 bits, portanto sua leitura pode ir de 0 até 4095. 
                              //Por este motivo, é necessário utilizar uma variável do tipo long para armazenar sua leitura.
  char InformacaoADCMQTT[10]; //array de chars que conterá a leitura do ADC convertida para texto.                       
  
  //verifica se deve fazer o envio (ou seja, se já decorreu ao menos TEMPO_ENVIO_ADC milisegundos desde o último envio)
  if (tempo_decorrido(TemporizacaoADC) < TEMPO_ENVIO_ADC)
  {
    //Se ainda nao se decorreu ao menos o tempo definido TEMPO_ENVIO_ADC, não é o momento de enviar. 
    //Portanto, se isso ocorrer, a função acaba aqui.
    return;
  }

  //Se chegou até aqui, é o momento de ler e enviar o ADC.
  //Portanto, faz a leitura do ADC, converte a leitura para texto e a envia por MQTT (no tópico "ADCCarlosKwiek")
  ValorLidoADC = analogRead(CANAL_ADC);
  memset(InformacaoADCMQTT,0,sizeof(InformacaoADCMQTT));
  sprintf(InformacaoADCMQTT,"%ld",ValorLidoADC);
  client.publish("ADCCarlosKwiek", InformacaoADCMQTT);

  //atualiza temporização de envio do ADC 
  //Dessa forma, após TEMPO_ENVIO_ADC milisegundos o valor do ADC será lido e enviado novamente.
  TemporizacaoADC = millis();


   float setpoint;
   setpoint = ValorLidoADC;
   setpoint = map(setpoint, 0, 1023, 0, 60);
   float Temperatura;
   Temperatura = dht.readTemperature();
  
   if (Temperatura >= setpoint){
   if (tempo_decorrido(TemporizacaoADC) < TEMPO_ENVIO_ADC)
   digitalWrite (smart,HIGH);
   client.publish("Ventilador", "Desligado"); 
   }
    else{digitalWrite (smart,LOW);
    client.publish("Ventilador", "Ligado"); 
   }
   
}

//Função: verifica se deve enviar via MQTT o valor da temperatura e umidade. Se sim, faz o envio.
//Parametros: nenhum
//Retorno: nenhum
void verifica_e_envia_temperatura_e_umidade(void)
{
  char TempMQTT[10];
  char UmidMQTT[10];
  float Temperatura;
  float Umidade;
  int TemperaturaInt;
  int UmidadeInt;
  
  //verifica se deve fazer o envio (ou seja, se já decorreu ao menos TEMPO_ENVIO_TEMP_UMIDADE milisegundos desde o último envio)
  if (tempo_decorrido(TemporizacaoTempUmid) < TEMPO_ENVIO_TEMP_UMIDADE)
  {
    //Se ainda nao se decorreu ao menos o tempo definido TEMPO_ENVIO_TEMP_UMIDADE, não é o momento de enviar. 
    //Portanto, se isso ocorrer, a função acaba aqui.
    return;
  }

  //Se chegou até aqui, é o momento de ler e enviar a temperatura e umidade
  //Portanto, faz a leitura destas informações, converte as leituras para texto e as envia por MQTT (tópico "TempCarlosKwiek" para temperatura e "UmidCarlosKwiek" para umidade)
  Temperatura = dht.readTemperature();
  Umidade = dht.readHumidity();

  //Importante: os ESP8266 não suportam conversão de valores float em texto. É uma limitação da implementação da função sprint para estes módulos. 
  //Por esta razão, transformei ( = trunquei) as informações para numero inteiro. Portanto, tanto a temperatura quanto umidades serão numeros inteiros.
  TemperaturaInt = (int)Temperatura;
  UmidadeInt = (int)Umidade;

  //se alguma das leituras não foi bem sucedida, nada mais é feito
  if (isnan(Temperatura) || isnan(Umidade) )
    return;

  //prepara e envia informações 
  //Atenção: a temperatura está em graus Celsius e a umidade é percentual (0-100)
  memset(TempMQTT,0,sizeof(TempMQTT));
  memset(UmidMQTT,0,sizeof(UmidMQTT));
  sprintf(TempMQTT,"%d",TemperaturaInt);
  sprintf(UmidMQTT,"%d",UmidadeInt);
  client.publish("TempCarlosKwiek", TempMQTT);
  client.publish("UmidCarlosKwiek", UmidMQTT);

  //atualiza temporização de envio de temperatura e umidade
  //Dessa forma, após TEMPO_ENVIO_TEMP_UMIDADE milisegundos os valores de temperatura e umidade serão lidos e enviados novamente.
  TemporizacaoTempUmid = millis();
}

//Função: máquina de estados do sensor PIR. Funcionamento:
//            - Aguarda o sensor detectar movimento
//            - Se detectado, aguarda X segundos (X definido em software) nesta condição para confirmar movimento
//            - Se confirmado o movimento, envia o alerta e aguarda próxima ocorrência de movimento.
//Parametros: nenhum
//Retorno: nenhum
void maquina_estados_sensor_PIR(void)
{
  switch (EstadoMaquinaSensorPIR)
  {
    case ESTADO_PIR_AGUARDA_MOVIMENTO:
    {
      if (digitalRead(PIRPin))
      {
        //foi detectado acionamento do sensor PIR
        TemporizacaoSensorPIR = millis();
        EstadoMaquinaSensorPIR = ESTADO_PIR_AGUARDA_CONFIRMACAO_MOVIMENTO;
      }
      
      break;
    }
    
    case ESTADO_PIR_AGUARDA_CONFIRMACAO_MOVIMENTO:
    {
      //verifica se o sensor continua acusando que há movimento
      if (!digitalRead(PIRPin))
      {
        //o sensor parou de acusar movimento. Reseta a maquina de estados.
        EstadoMaquinaSensorPIR = ESTADO_PIR_AGUARDA_MOVIMENTO;
        break;
      }
      
      //verifica se ja deu o tempo de confirmação de movimento
      if ( (digitalRead(PIRPin)) && (tempo_decorrido(TemporizacaoSensorPIR) >= TEMPO_CONFIRMACAO_MOVIMENTO) )
      {
        //movimento confirmado. Envia mensagem MQTT
        client.publish("PIRCarlosKwiek", "1");
        EstadoMaquinaSensorPIR = ESTADO_PIR_AGUARDA_FIM_DE_MOVIMENTO;
      }   
      
      break;
    }
    
    case ESTADO_PIR_AGUARDA_FIM_DE_MOVIMENTO:
    {
      //aguarda o sensor parar de indicar movimento e reinicia maquina de estados
      if (!digitalRead(PIRPin))
      {       
        //acabou o movimento. reinicia máquina de estados e envia mensagem MQTT
        client.publish("PIRCarlosKwiek", "0");      
        EstadoMaquinaSensorPIR = ESTADO_PIR_AGUARDA_MOVIMENTO;
      }
      break;
    }

  }
}
  
void setup(void) {
    //configura o botao
    pinMode(buttonPin, INPUT_PULLUP);

    //configura a saida do LED
    pinMode(ledPin, OUTPUT);
    digitalWrite(ledPin,LOW);
    EstadoLED = 0;
    pinMode(smart, OUTPUT);

    //inicializa temporizações de envio do ADC, temperatura, umidade e sensor PIR
    TemporizacaoADC = millis();
    TemporizacaoTempUmid = millis();
    TemporizacaoSensorPIR = millis();
  
    //inicializa sensor de temperatura
    dht.begin();
  
    //inicializa máquina de estados do sensor PIR e pino de entradas
    pinMode(PIRPin, INPUT);
    EstadoMaquinaSensorPIR = ESTADO_PIR_AGUARDA_MOVIMENTO;    
    
    //configura comunicação serial e faz conexao ao broker mqtt
    Serial.begin(115200);
    connecta_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    //envia estado inicial de movimento
    if (!digitalRead(PIRPin))
        client.publish("PIRCarlosKwiek", "0");  
    else    
        client.publish("PIRCarlosKwiek", "1");  
}   
   
void loop() 
{
    //Faz tratamentos de rede e mqtt
    if (!client.connected()) 
       reconecta_mqtt();
   
    reconecta_wifi();   
    
    //verifica acionamento do botao em D1
    verifica_botao();

    //verifica se deve ler o ADC e enviar valor da leitura via MQTT
    verifica_e_envia_valor_potenciometro();

    //verifica se deve ler o sensor de temperatura e umidade e enviar tais informações via MQTT
    verifica_e_envia_temperatura_e_umidade();
  
    //executa a maquina de estados do sensor PIR
    maquina_estados_sensor_PIR();  
  
    //envia keep-alive MQTT
    client.loop();
     
}
