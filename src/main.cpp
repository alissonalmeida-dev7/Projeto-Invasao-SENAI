/*
Nome:
Data:
Programa:
Descrição:
Versão: 1.0
*/

// ----------Bibliotecas-----------
#include <Arduino.h>
#include "WiFiManager.h"
#include "MQTTManager.h"
#include "DebugManager.h"
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <LED.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <Bounce2.h>
#include "time.h"


// ----------Configurações de Tempo----------
const char* servidorNTP = "pool.ntp.org"; // Servidor mundial de hora.
const long gmtOffset_sec = -3 * 3600; // Ajuste para o horario do Brasil.
const int daylightOffset_sec = 0; // Horário de verâo (atualmente 0).
String ultimaHoraLida = "";

// ----------Configurações Whatsapp----------
String numeroUsuario = "+5511911539131";
String CHAVE_API = "4923782";

// ----------Segurança e Controle----------
String senhaMestra = "1981";
int tentativasSenha = 0;
bool foiNotificado = false; // Trava para não repetir Whatsapp sem parar

// ----------Pinos----------
const int PIN_LED_RGB = 48;
const int QNTD_LEDS = 1;
const int PIN_BUZZER = 18; 
LED lampada(46);

// ----------Estados e Tópicos MQTT----------
const char TOPICO_COMANDO[] = "sistema/estado";
String estadoSistema = "desativado";
unsigned long tempoInicioPreAlerta = 0;
const unsigned long TEMPO_LIMITE_PIN_MS = 10000;

// ----------LED RGB----------
Adafruit_NeoPixel ledRGB(
  QNTD_LEDS, 
  PIN_LED_RGB, 
  NEO_GRB + NEO_KHZ800
);
float brilhoPulso = 0;
float incrementoPulso = 5.0;

// ----------LCD----------
LiquidCrystal_I2C lcd(0x27, 20, 4);
Bounce botao = Bounce();

// ----------Protótipos----------
void tratarMensagemRecebida(const char *topico, const String &mensagem);
void configurarLedRGB();
void alterarCorLedRGB(int red, int green, int blue);
void tratarJsonComando(const String &mensagem);
void atualizarLCD(String linha1, String linha2, String linha3, String linha4);
void enviarMensagemWhatsapp(String mensagem);
void dispararInvasao();
String obterDataHora();
void gerenciarRelogioNoLCD();


// ----------Setup----------
void setup() 
{
  lcd.init();
  lcd.backlight();
  atualizarLCD("SISTEMA INICIANDO", "Aguarde WiFi...", "", "");
  
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER); // Garante silêncio inicial

  configurarDebug();
  conectarWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, servidorNTP);

  botao.attach(35, INPUT_PULLUP);
  botao.interval(10);

  configurarMQTT();
  registrarCallbackMensagem(tratarMensagemRecebida);
  conectarMQTT();
  configurarLedRGB();

  atualizarLCD("SISTEMA OFF", "Acesso Livre", "", obterDataHora());
}

// ----------Loop----------
void loop() 
{
  botao.update();
  gerenciarRelogioNoLCD();

  // Reset físico pelo botão
  if(botao.fell()) 
  {
    if(estadoSistema == "invasao" || estadoSistema == "pre_alerta") 
    {
      estadoSistema = "ativado";
      foiNotificado = false; 
      noTone(PIN_BUZZER); // Desliga o som
      alterarCorLedRGB(0, 255, 0);
      lampada.apagar();
      atualizarLCD("SISTEMA ATIVADO", "Vigiando...", "", obterDataHora());
      debugInfo("Reset fisico realizado.");
    }
  }

  // Lógica de estouro de tempo do PIN
  if(estadoSistema == "pre_alerta") 
  {
    if(millis() - tempoInicioPreAlerta >= TEMPO_LIMITE_PIN_MS) 
    {
      dispararInvasao();
    }
  }

  garantirWiFiConectado();
  garantirMQTTConectado();
  MQTTLoop();
  lampada.update();

  // Efeito de Sirene Visual e Sonora
  if(estadoSistema == "invasao") 
  {
    brilhoPulso += incrementoPulso;
    // Se chegar no brilho máximo (255) ou mínimo (0), inverte a direção
    if(brilhoPulso >= 255 || brilhoPulso <= 0) 
      incrementoPulso = -incrementoPulso;
    
    alterarCorLedRGB(constrain(brilhoPulso, 0, 255), 0, 0);

    // Buzzer acompanha o pulso do LED para efeito de sirene
    if(brilhoPulso > 150) 
    {
      tone(PIN_BUZZER, 2000); // Frequência de 2kHz
    } 
    
    else 
    {
      noTone(PIN_BUZZER);
    }
    
    delay(5); // Pequeno delay para a transiçâo ficar suave (5ms).
  } 
  
  else 
  {
    noTone(PIN_BUZZER); // Garante silêncio se não houver invasão
  }
}

// ----------Tratar comandos recebidos do JSON----------
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
      foiNotificado = false;
      noTone(PIN_BUZZER);
      alterarCorLedRGB(0, 255, 0);
      lampada.apagar();
      atualizarLCD("SISTEMA ATIVADO", "Vigiando...", "", obterDataHora());
    }

    else if(comando == "desativado") 
    {
      estadoSistema = "desativado";
      foiNotificado = false;
      noTone(PIN_BUZZER);
      alterarCorLedRGB(0, 0, 0);
      lampada.apagar();
      atualizarLCD("SISTEMA DESATIVADO", "Acesso Livre", "", obterDataHora());
    }

    else if(comando == "reset") 
    {
      if(estadoSistema == "invasao" || estadoSistema == "pre_alerta") 
      {
        if(doc["pin"].as<String>() == senhaMestra) 
        {
          estadoSistema = "ativado";
          tentativasSenha = 0;
          foiNotificado = false; 
          noTone(PIN_BUZZER); // CALA O BUZZER
          lampada.apagar();
          alterarCorLedRGB(0, 255, 0);
          atualizarLCD("ACESSO AUTORIZADO", "Alarme Resetado", "", obterDataHora());
        } 
        
        else 
        {
          if(estadoSistema == "pre_alerta") 
          {
            dispararInvasao(); 
          } 
          
          else 
          {
            tentativasSenha++;
            if(tentativasSenha >= 2) 
            {
              enviarMensagemWhatsapp("⚠️ Tentativa de reset com senha incorreta!");
              atualizarLCD("!!! INVASAO !!!", "SENHA INCORRETA", "AVISANDO DONO...", obterDataHora());            
            } 

            else 
            {
              atualizarLCD("!!! INVASAO !!!", "SENHA INCORRETA", "TENTE NOVAMENTE...", obterDataHora());
            }
          }
        }
      }
    }
  }

  // Detecção de movimento via MQTT
  if(doc["violacao"].is<bool>() && doc["violacao"].as<bool>() == true) 
  {
    if(estadoSistema == "ativado") 
    {
      estadoSistema = "pre_alerta";
      tempoInicioPreAlerta = millis();
      alterarCorLedRGB(255, 255, 0); 
      atualizarLCD("MOVIMENTO DETECTADO", "Digite o PIN", "Tempo: 10 segundos", obterDataHora());
      
      if(!foiNotificado) 
      {
        enviarMensagemWhatsapp("🚨 Movimentacao suspeita detectada!");
        foiNotificado = true; 
      }
    }
  }
}

// ----------Função: Ativar Invasâo no Sistema----------
void dispararInvasao() 
{
  estadoSistema = "invasao";
  alterarCorLedRGB(255, 0, 0);
  lampada.piscar(0.5f);
  
  // Se já notificou suspeita, agora confirma a invasão
  if(foiNotificado) 
  { 
     enviarMensagemWhatsapp("🚨🚨 INVASAO CONFIRMADA NO SETOR!");
     foiNotificado = false; // Libera a trava para o próximo ciclo de segurança
  }
  
  atualizarLCD("!!! INVASAO !!!", "ALERME DISPARADO", "Dono Notificado", obterDataHora());
}

// ----------Função: Enviar Mensagens pelo Callbot----------
void enviarMensagemWhatsapp(String mensagem) 
{
  mensagem.replace(" ", "+");
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + numeroUsuario + "&text=" + mensagem + "&apikey=" + CHAVE_API;

  WiFiClientSecure client;
  client.setInsecure(); 

  HTTPClient http;
  http.setTimeout(5000); // Timeout de 5s para não travar o código se o site estiver lento
  if (http.begin(client, url)) 
  {
    int codigoResposta = http.GET();
    debugInfo("WhatsApp Status: " + String(codigoResposta));
    http.end();
  }
}

// ----------Função: Atualizar o LCD----------
void atualizarLCD(String linha1, String linha2, String linha3, String linha4) 
{
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(linha1);
  lcd.setCursor(0, 1); lcd.print(linha2);
  lcd.setCursor(0, 2); lcd.print(linha3);
  lcd.setCursor(0, 3); lcd.print(linha4);
}

// ----------Tratamento de Topicos----------
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

// ----------Função: Configura o LED RGB----------
void configurarLedRGB() 
{
  ledRGB.begin();
  ledRGB.setBrightness(80);
  ledRGB.clear();
  ledRGB.show();
  debugInfo("LED RGB configurado no GPIO " + String(PIN_LED_RGB));
}

// ----------Função: Altera o LED RGB----------
void alterarCorLedRGB(int vermelho, int verde, int azul) 
{
  ledRGB.setPixelColor(0, ledRGB.Color(
    constrain(vermelho, 0, 255), 
    constrain(verde, 0, 255), 
    constrain(azul, 0, 255))
  );
  ledRGB.show();
}

// ----------Função: Obtem Data e Horas----------
String obterDataHora() 
{
  // Modelo de ficha que tem um formato de relogio completo: dd/mm/aaaa hh:mm
  struct tm timeinfo;  
  if(!getLocalTime(&timeinfo)) 
    return "Erro ao ler hora";
  
  char buffer[20];
  // %d = dia, %m = mês, %y = ano, %H = hora, %M = minuto
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", &timeinfo);
  return String(buffer);
}

// ----------Função: Atualiza os minutos----------
void gerenciarRelogioNoLCD() {
  String horaAtual = obterDataHora();
  
  // Só atualiza o LCD se a string de hora for diferente da última que mostramos
  if (horaAtual != ultimaHoraLida) {
    lcd.setCursor(0, 3); // Vai direto para a linha 4 (Linha de data e hora)
    lcd.print(horaAtual); // Escreve por cima do que estava lá
    ultimaHoraLida = horaAtual; 
    debugInfo("Relógio atualizado no LCD");
  }
}


/* JSON

{
  "sistema": "ativado",
  "violacao": false,
  "pin": "0000"
}

*/