#include <Arduino.h>
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "DebugManager.h"
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LED.h>
#include <LiquidCrystal_I2C.h>

const int PIN_LED_RGB = 48;
const int QNTD_LEDS = 1;
const char TOPICO_COMANDO[] = "esp32/comando";
const int PIN_LAMPADA = 46;

bool movimento = LOW;

Adafruit_NeoPixel ledRGB(
    QNTD_LEDS,
    PIN_LED_RGB,
    NEO_GRB + NEO_KHZ800 // --> Igual a 82
);

LiquidCrystal_I2C lcd(0x27, 20, 4);

/*
Nome:
Data:
Programa:
Descrição:
Versão: 1.0
*/

void tratarMensagemRecebida(const char *topico, const String &mensagem);
void configurarLedRGB();
void alterarCorLedRGB(int red, int green, int blue);
void tratarJsonComando(const String &mensagem);
void alterarLampada(bool estado);

void setup()
{

  lcd.init();
  lcd.backlight();

  configurarDebug();
  conectarWiFi();
  configurarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  conectarMQTT();
  pinMode(PIN_LAMPADA, OUTPUT);
}

void loop()
{
  garantirWiFiConectado();
  garantirMQTTConectado();
  MQTTLoop();
}

void tratarMensagemRecebida(const char *topico, const String &mensagem)
{
  debugInfo("=======================================");
  debugInfo("Mensagem recebida da aplicação");
  debugInfo("=======================================");

  if (topico == nullptr)
  {
    debugErro("Tópico MQTT inválido");
    return;
  }

  debugInfo("Tópico: " + String(topico));
  debugInfo("Mensagem: " + mensagem);

  if (strcmp(topico, TOPICO_COMANDO) == 0)
  {
    tratarJsonComando(mensagem);
    return;
  }
  debugErro("Tópico não tratado: " + String(topico));
}

void configurarLedRGB()
{

  ledRGB.begin();
  ledRGB.setBrightness(80);
  ledRGB.clear();
  ledRGB.show();
  debugInfo("LED RGB configurado no GPIO " + String(PIN_LED_RGB));
}

void alterarCorLedRGB(int vermelho, int verde, int azul)
{
  constrain(vermelho, 0, 255);
  constrain(verde, 0, 255);
  constrain(azul, 0, 255);

  ledRGB.setPixelColor(0, ledRGB.Color(vermelho, verde, azul));
  ledRGB.show();
  debugInfo("Cor aplicada no LED RGB");
  debugInfo("R: " + String(vermelho));
  debugInfo("G: " + String(verde));
  debugInfo("B: " + String(azul));
}

void tratarJsonComando(const String &mensagem)
{
  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, mensagem);
  if (erro)
  {
    debugErro("Erro ao deserializar JSON, código: " + String(erro.c_str()));
    return;
  }
  if (!doc["led"].is<JsonObject>())
  {
    debugErro("JSON inválido, não contém chave para 'led' ");
    return;
  }
  if (!doc["led"]["r"].is<int>() || !doc["led"]["g"].is<int>() || !doc["led"]["b"].is<int>())
  {
    debugErro("JSON Inválido, não foi possível encontrar chave para 'r' ou 'g' ou 'b' ");
    return;
  }

  if (doc["sensor_virtual"].is<bool>())
  {
    debugInfo("Processando lampada");
    alterarLampada(doc["sensor_virtual"].as<bool>());
  }

}

void alterarLampada(bool estado)
{
  if (estado)
  {
    debugInfo("Ligando lampada");
    digitalWrite(PIN_LAMPADA, HIGH);
  }
  else
  {
    debugInfo("Desligando lampada");
    digitalWrite(PIN_LAMPADA, LOW);
  }
}

void configuracaoDisplay()
{ // com movimento
  if (movimento == HIGH)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Movimento detectado");
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sem movimento");
  }
}

/*
 FORMATO JSON
{
"led": {
  "r": 255,
  "g": 0,
  "b":
  }
}
*/