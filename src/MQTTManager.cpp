#include <Arduino.h>
#include "DebugManager.h"
#include "secrets.h"

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "MqttManager.h"
#include "WiFiManager.h"
#include "MqttManager.h"

//========== Instâncias =================
WiFiClient WifiCliente;
WiFiClientSecure WifiClienteSecure;
PubSubClient MqttClient;

callbackMensagemMQTT callBackDoApp = nullptr;

void registrarCallbackMensagem(callbackMensagemMQTT callback)
{

    callBackDoApp = callback;

    if (callBackDoApp != nullptr)
    {
        debugInfo("Callback da aplicação registrada com sucesso");
    }
    else
    {
        debugInfo("Callback da aplicação não foi registrada");
    }
}

const char *obterTopicoPublicacao(int indiceTopico)
{
    if (indiceTopico < 0 || indiceTopico >= TOTAL_TOPICOS_PUBLICAR)
    {
        debugErro("Indice inválido para tópico de publicação: " + String(indiceTopico));
        return "";
    }
    return TOPICOS_PUBLICAR[indiceTopico];
}

const char *obterTopicoRecebimento(int indiceTopico)
{
    if (indiceTopico < 0 || indiceTopico >= TOTAL_TOPICOS_RECEBER)
    {
        debugErro("Indice inválido para tópico de recebimento: " + String(indiceTopico));
        return "";
    }
    return TOPICOS_RECEBER[indiceTopico];
}

void callbackInternoMQTT(char *topico, byte *payload, unsigned int tamanho)
{

    String mensagem = "";
    for (unsigned int i = 0; i < tamanho; i++)
    {
        mensagem += (char)payload[i];
    }
    debugInfo("=======================");
    debugInfo(" MENSAGEM MQTT RECEBIDA");
    debugInfo("Topico: " + String(topico));
    debugInfo("Mensagem: " + mensagem);

    if (callBackDoApp != nullptr)
    {
        callBackDoApp(topico, mensagem);
    }
    else
    {
        debugErro("Mensagem recebida, porém nenhum callback da aplicação foi registrado");
    }
}

void configurarMQTT()
{
    debugInfo("=======================");
    debugInfo("Configurando MQTT...");
    debugInfo("");

    if (USAR_AWS_IOT)
    {
        // TODO IMPLEMENTAR CONEXÃO AWS
    }
    else if (MQTT_USAR_TLS)
    {
        // TODO IMPLEMENTAR CONEXÁO COM CERTIFICADO

        debugInfo("Modo selecionado: MQTT com TLS");

        if (strlen(MQTT_CERTIFICADO_CA) > 100)
        {
            debugInfo("Certificado CA do broker MQTT configurado");
            WifiClienteSecure.setCACert(MQTT_CERTIFICADO_CA); // CA = autoridade certificadora
        }
        else
        {
            debugErro("Certificado CA não foi configurado. Usando setInsecure apenas para testes.");
            WifiClienteSecure.setInsecure();
        }
        
        MqttClient.setClient(WifiClienteSecure);
        MqttClient.setServer(MQTT_BROKER, MQTT_PORTA);

        debugInfo("Broker MQTT" + String(MQTT_BROKER));
        debugInfo("Porta MQTT: " + String(MQTT_PORTA));
    }
    else
    { // Conectar no broker sem certificado
        debugInfo("Modo Selecionado: MQTT sem TLS");
        MqttClient.setClient(WifiCliente);
        MqttClient.setServer(MQTT_BROKER, MQTT_PORTA);

        debugInfo("Broker MQTT" + String(MQTT_BROKER));
        debugInfo("Porta MQTT: " + String(MQTT_PORTA));
    }
    MqttClient.setCallback(callbackInternoMQTT);
    debugInfo("Callback interno do MQTT Configurado");
}

void conectarMQTT()
{
    if (!wifiConectado())
    {
        debugInfo("MQTT não pode iniciar sem conexão wifi");
    }
    debugInfo("===========================");
    debugInfo("Conectando MQTT");
    debugInfo("===========================");
    int tentativasMQTT = 0;
    const int maxTentativasMQTT = 5;
    while (!MqttClient.connected() && tentativasMQTT < maxTentativasMQTT)
    {
        debugInfo("Tentando conectar ao MQTT Broker");
        bool conectado = false;

        if (USAR_AWS_IOT)
        {
            // TODO Implementar futuramente
        }
        else
        {
            if (strlen(MQTT_USUARIO) > 0)
            {
                debugInfo("Conectando MQTT com usuário e senha");
                conectado = MqttClient.connect(MQTT_CLIENT_ID, MQTT_USUARIO, MQTT_SENHA);
            }
            else
            {
                debugInfo("Conectando MQTT sem usuário e senha");
                conectado = MqttClient.connect(MQTT_CLIENT_ID);
            }
        }
        if (conectado)
        {
            debugInfo("Conectado ao MQTT com sucesso");
            int totalTopicos = obterTotalTopicosRecebimentos();
            debugInfo("Total de tópicos para inscrição: " + String(totalTopicos));
            for (int i = 0; i < totalTopicos; i++)
            {
                const char *topico = obterTopicoRecebimento(i);
                bool inscrito = MqttClient.subscribe(topico);
                if (inscrito)
                    debugInfo("Inscrito com sucesso no tópico com sucesso: " + String(topico));
                else
                    debugInfo("Falha ao se conectar no tópico: " + String(topico));
            }
            // TODO Publicar uma mensagem em um tópico informando que o ESP32 foi conectado.
        }
        else
        {
            debugErro("Falha ao conectar ao MQTT, código erro: " + String(MqttClient.state()));
            tentativasMQTT++;
            delay(2000);
        }
    }
    if (!MqttClient.connected())
    {
        debugErro("Não foi conectar ao MQTT,após " + String(tentativasMQTT) + "tentativas, não haverá tentativas futuras");
    }
}

void garantirMQTTConectado()
{
    if (!wifiConectado)
    {
        debugErro("MQTT desconectado, não será reconectado porquê o wifi está desconectado");
        return;
    }
    if (!MqttClient.connected())
    {
        debugErro("MQTT desconectado, tentando conectar.");
        conectarMQTT();
    }
}
void MQTTLoop()
{
    MqttClient.loop();
}

void publicarMensagem(const char *topico, const char *mensagem)
{
    if (!MqttClient.connected())
    {
        debugErro("MQTT não está conectado, não sera possível publicar mensagem");
        return;
    }
    bool publicado = MqttClient.publish(topico, mensagem);

    if (publicado)
    {
        debugInfo("Mensagem publicada via MQTT com sucesso");
        debugInfo("Tópico: " + String(topico));
        debugInfo("Mensagem " + String(mensagem));
    }
    else
    {
        debugErro("Falha ao publicar mensagem no tópico: " + String(topico));
    }
}

void publicarMesagemNoTopico(int indiceTopico, const char *mensagem)
{
    const char *topico = obterTopicoPublicacao(indiceTopico);
    if (strlen(topico) == 0)
    {
        debugErro("Não foi possível publicar indice de tópico inválido: " + String(indiceTopico));
        return;
    }
    publicarMensagem(topico, mensagem);
}

bool mqttConectado()
{
    return MqttClient.connected();
}

int obterTotalTopicosRecebimentos()
{
    return TOTAL_TOPICOS_RECEBER;
}