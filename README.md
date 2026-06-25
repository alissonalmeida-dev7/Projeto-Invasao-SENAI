# Sistema Anti-Invasão com ESP32

Desenvolvido no SENAI Paulo Antonio Skaf como projeto de conclusão de módulo.

**Equipe**

- [Alisson Almeida Gomes](https://github.com/alissonalmeida-dev7)
- [Fabricio Azevedo Almeida](https://github.com/FabricioAzevedoAlmeida)
- [Heloísa Tomé de Araujo](https://github.com/hyopsywan)
- [Kael Fontes Araujo](https://github.com/wKaelzx)
- [Luis Otávio Coelho Ferreira](https://github.com/luisoferreira)
- [Victor Bueno](https://github.com/Vbueno04)

---

Sistema de segurança simulado com ESP32-S3 que monitora um ambiente e reage a movimentos detectados via MQTT. O estado é exibido em tempo real num LCD 20x4, com LED RGB, buzzer e notificação por WhatsApp.

## Estados do sistema

| Estado | Descrição |
|---|---|
| Desativado | Sistema inativo, acesso livre |
| Ativado | Vigiando o ambiente |
| Pré-alerta | Movimento detectado, aguarda PIN por 10 segundos |
| Invasão | Alarme disparado, dono notificado via WhatsApp |

## Funcionalidades

**Segurança**
- Detecção de movimento via tópico MQTT (`violacao: true`)
- Pré-alerta com contagem regressiva de 10 segundos via `millis()`
- Senha mestra para reset do alarme, com limite de tentativas
- Trava contra notificações duplicadas no mesmo ciclo (`foiNotificado`)
- Reset físico pelo botão do ESP32 (Bounce2, pino 35)

**Notificação**
- Envio de mensagens via WhatsApp usando a API do CallMeBot (HTTPS)
- Conexão com `WiFiClientSecure` e timeout de 5 segundos
- Notifica suspeita ao detectar movimento e confirma invasão caso o PIN não seja digitado

**Feedback visual e sonoro**
- LED RGB NeoPixel indicando o estado: verde (ativado), amarelo (pré-alerta), vermelho pulsante (invasão)
- Buzzer em 2kHz sincronizado ao pulso do LED, formando efeito de sirene
- LED externo piscando durante invasão

**Display e tempo**
- LCD 20x4 I2C exibindo estado, mensagens e horário
- Sincronização de horário via protocolo NTP (`pool.ntp.org`, UTC-3, sem horário de verão)
- Relógio atualizado automaticamente na linha 4 do LCD sem bloquear o loop

**Rede**
- Conexão WiFi com reconexão automática
- MQTT com TLS (porta 8883) via HiveMQ Cloud, com reconexão automática no loop
- Certificado CA configurado via `PROGMEM`

## Hardware

| Componente | Pino |
|---|---|
| ESP32-S3 DevKit | — |
| LED RGB NeoPixel | 48 |
| Buzzer | 18 |
| LED externo | 46 |
| LCD I2C 20x4 | 0x27 |
| Botão de reset | 35 |

## MQTT

Broker HiveMQ Cloud, porta 8883 (TLS). Tópico: `sistema/estado`

```json
{
  "sistema": "ativado",
  "violacao": false,
  "pin": "0000"
}
```

Valores de `sistema`: `ativado` · `desativado` · `reset`

## Como rodar

1. Configure `src/secrets.cpp` com suas credenciais de WiFi, MQTT e WhatsApp (CallMeBot)
2. Abra no VSCode com PlatformIO
3. Faça upload para o ESP32-S3
4. Envie o JSON pelo broker para o tópico `sistema/estado`

## Dependências

```
knolleary/PubSubClient@^2.8
bblanchon/ArduinoJson@7.4.3
adafruit/Adafruit NeoPixel@^1.15.4
marcoschwartz/LiquidCrystal_I2C@^1.1.4
thomasfredericks/Bounce2@^2.72
https://github.com/wKaelzx/LED.h-Biblioteca
```
