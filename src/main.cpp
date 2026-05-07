/*
Nome:
Data:
Programa:
Descrição:
Versão: 1.0
*/

#include <Arduino.h>
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "DebugManager.h"
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LED.h>
#include <LiquidCrystal_I2C.h>

#include <HTTPClient.h>
#include <Bounce2.h>
#include "time.h"

const char* servidorNTP = "pool.ntp.org";  // Servidor mundial de hora.
const long gmtOffset_sec = -3 * 3600;  // Ajuste para o horario do Brasil.
const int daylightOffset_sec = 0;  // Horário de verâo (atualmente 0).
String obterDataHora();

String numeroUsuario = "+5511932583291";
String CHAVE_API = "7942559";

String senhaMestra = "1981";
int tentativasSenha = 0;

const int PIN_LED_RGB = 48;
const int QNTD_LEDS = 1;
const char TOPICO_COMANDO[] = "sistema/estado";

LED lampada(46);

String estadoSistema = "desativado";

Adafruit_NeoPixel ledRGB(
    QNTD_LEDS,
    PIN_LED_RGB,
    NEO_GRB + NEO_KHZ800 // --> Igual a 82
);

float brilhoPulso = 0;
float incrementoPulso = 5.0;

LiquidCrystal_I2C lcd(0x27, 20, 4);

Bounce botao = Bounce();

void tratarMensagemRecebida(const char *topico, const String &mensagem);
void configurarLedRGB();
void alterarCorLedRGB(int red, int green, int blue);
void tratarJsonComando(const String &mensagem);
void atualizarLCD(String linha1, String linha2, String linha3, String linha4);
void enviarMensagemWhatsapp(String mensagem);

void setup()
{
  lcd.init();
  lcd.backlight();


  configTime(gmtOffset_sec, daylightOffset_sec, servidorNTP);

  botao.attach(0, INPUT_PULLUP);
  botao.interval(10);

  configurarDebug();
  conectarWiFi();
  configurarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  conectarMQTT();
  configurarLedRGB();

  atualizarLCD("SISTEMA OFF", "Acesso Livre", "", obterDataHora());
}

void loop()
{
  botao.update();

  if(botao.fell())
  {
    if(estadoSistema == "invasao")
    {
      estadoSistema = "ativado";
      alterarCorLedRGB(0, 255, 0);
      lampada.apagar();
      atualizarLCD("SISTEMA ATIVADO", "Vigiando...", "", obterDataHora());
      debugInfo("Alarme resetado. Sistema voltou a vigiar.");
    }

    else
    {
      debugInfo("Comando de reset ignorado (Sistema nao esta em invasao).");
    }
  }

  garantirWiFiConectado();
  garantirMQTTConectado();
  MQTTLoop();

  lampada.update();
  if(estadoSistema == "invasao")
  {
    brilhoPulso += incrementoPulso;

    // Se chegar no brilho máximo (255) ou mínimo (0), inverte a direção
    if(brilhoPulso >= 255 || brilhoPulso <= 0) 
    {
      incrementoPulso = -incrementoPulso;
    }

    alterarCorLedRGB(constrain(brilhoPulso, 0, 255), 0, 0);
    delay(5);  // Pequeno delay para a transiçâo ficar suave (5ms).
  }
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

  if(erro)
  {
    debugErro("Erro ao deserializar JSON, código: " + String(erro.c_str()));
    return;
  }

  if(doc["sistema"].is<String>())
  {
    String comando = doc["sistema"].as<String>();

    if(comando == "ativado")
    {
      estadoSistema = "ativado";
      alterarCorLedRGB(0, 255, 0);
      lampada.apagar();
      atualizarLCD("SISTEMA ATIVADO", "Vigiando...", "", obterDataHora());
      debugInfo("O sistema foi ATIVADO");
    }

    else if(comando == "desativado")
    {
      estadoSistema = "desativado";
      alterarCorLedRGB(0, 0, 0);
      lampada.apagar();
      atualizarLCD("SISTEMA DESATIVADO", "Acesso Livre", "", obterDataHora());
      debugInfo("O sistema foi DESATIVADO");
    }

    else if(comando == "reset")
    {
      if(estadoSistema == "invasao")
      {
        String pinRecebido = doc["pin"].as<String>();

        if(pinRecebido == senhaMestra)
        {
          estadoSistema = "ativado";
          tentativasSenha = 0;
          alterarCorLedRGB(0, 255, 0);
          lampada.apagar();
          atualizarLCD("SISTEMA ATIVADO", "Vigiando...", "", obterDataHora());
          debugInfo("Alarme resetado. Sistema voltou a vigiar.");
        }

        else
        {
          tentativasSenha++;

          if(tentativasSenha >= 2)
          {
            enviarMensagemWhatsapp("⚠️ Alerta: Tentativa de reset com senha incorreta!");
            atualizarLCD("!!! INVASAO !!!", "SENHA INCORRETA", "AVISANDO DONO...", obterDataHora());            
          }

          else 
          {
            atualizarLCD("!!! INVASAO !!!", "SENHA INCORRETA", "TENTE NOVAMENTE...", obterDataHora());
          }
        }
      }

      else
      {
        debugInfo("Comando de reset ignorado (Sistema nao esta em invasao).");
      }
    }
  } 

  if(doc["violacao"].is<bool>() && doc["violacao"].as<bool>() == true)
  {
    if(estadoSistema == "ativado")
    {
      estadoSistema = "invasao";
      atualizarLCD("!!! INVASAO !!!", "Setor Violado", "Aguardando Senha...", obterDataHora());
      lampada.piscar(1.0f);
      debugInfo("=> ALERTA CRITICO: INVASAO DETECTADA!");
      enviarMensagemWhatsapp("🚨🚨 Alerta!!! Movimentacao suspeita detectada");
    }

    else
    {
      debugInfo("Movimento detectado, mas ignorado (Sistema OFF).");
    }
  }

}

void atualizarLCD(String linha1, String linha2, String linha3, String linha4)
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(linha1);
    lcd.setCursor(0, 1);
    lcd.print(linha2);
    lcd.setCursor(0, 2);
    lcd.print(linha3);
    lcd.setCursor(0, 3);
    lcd.print(linha4);
}

void enviarMensagemWhatsapp(String mensagem)
{
  mensagem.replace(" ", "+");
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + numeroUsuario + "&text=" + mensagem + "&apikey=" + CHAVE_API;
  HTTPClient http;
  http.begin(url);

  int codigoResposta = http.GET();
  
  if(codigoResposta > 0)
  {
    debugInfo("Whatsapp enviado com sucesso! Resposta do Servidor: " + String(codigoResposta));
  }

  else
  {
    debugErro("Erro ao enviar Whatsapp. Codigo de erro interno: " + String(codigoResposta));
  }

  http.end();
}

String obterDataHora()
{
  // Modelo de ficha que tem um formato de relogio completo: dd/mm/aaaa hh:mm
  struct tm timeinfo;  
  if(!getLocalTime(&timeinfo))
  {
    return "Erro ao ler hora";  // Caso o Wi-Fi caia.
  }
  char buffer[20];
  // %d = dia, %m = mês, %y = ano, %H = hora, %M = minuto
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

/* JSON

{
  "sistema": "ativado",
  "violacao": false,
  "pin": "0000"
}

*/