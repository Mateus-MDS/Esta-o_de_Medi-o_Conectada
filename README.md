# Esta��o Meteorol�gica Completa com Raspberry Pi Pico

## Por: Mateus Moreira da Silva

Este projeto implementa uma esta��o meteorol�gica completa utilizando o microcontrolador **RP2040** (Raspberry Pi Pico), sensores digitais de temperatura/umidade (**AHT20**), press�o/temperatura (**BMP280**), display OLED I2C (**SSD1306**), matriz de LEDs, buzzer, LEDs RGB e conectividade Wi-Fi.

## Funcionalidades

- Leitura simult�nea de temperatura e umidade (AHT20)
- Leitura de press�o, temperatura e c�lculo de altitude (BMP280)
- C�lculo de temperatura m�dia entre os dois sensores com calibra��o por offset
- Exibi��o das informa��es em tempo real no display OLED SSD1306 com 4 p�ginas naveg�veis
- Sistema de alertas visuais e sonoros quando os sensores ultrapassam limites m�ximos:
  - LEDs RGB (verde = normal, vermelho = alerta)
  - Buzzer com som intermitente
  - Matriz de LEDs 5x5 mostrando status das grandezas (T, U, P)
- Conectividade Wi-Fi com servidor HTTP para acesso remoto
- Comunica��o via barramento I2C com m�ltiplos dispositivos
- Sa�da dos dados para o terminal serial (debug/log)
- Navega��o entre p�ginas do display com bot�o A
- Interface web para monitoramento remoto

---

## Hardware Necess�rio

- Raspberry Pi Pico W (RP2040 com Wi-Fi)
- Sensor AHT20 ? I2C (temperatura e umidade)
- Sensor BMP280 ? I2C (press�o e temperatura)
- Display OLED SSD1306 (128x64) ? I2C
- Matriz de LEDs RGB 5x5 endere��vel (WS2812B)
- Buzzer ativo
- 2x LEDs (verde e vermelho)
- Bot�o para navega��o
- Resistores de pull-up
- Cabos de conex�o

---

## Conex�es (GPIO)

| Dispositivo              | Barramento | SDA/GPIO | SCL/GPIO | Endere�o I2C padr�o           |
|--------------------------|------------|----------|----------|-------------------------------|
| AHT20 + BMP280           | i2c0       | 4        | 5        | 0x38 (AHT20), 0x76 (BMP280) |
| SSD1306 Display          | i2c1       | 14       | 15       | 0x3C                         |
| Matriz LEDs (WS2812B)    | -          | 7        | -        | -                            |
| LED Verde                | -          | 11       | -        | -                            |
| LED Vermelho             | -          | 12       | -        | -                            |
| Buzzer                   | -          | 21       | -        | -                            |
| Bot�o A (Navega��o)      | -          | 5        | -        | -                            |

---

## P�ginas do Display

O display possui 4 p�ginas naveg�veis atrav�s do bot�o A:

### P�gina 1: Dados Principais
- Temperatura m�dia (�C)
- Umidade (%)
- Press�o (kPa)

### P�gina 2: Limites M�ximos
- Temperatura m�xima configurada
- Umidade m�xima configurada
- Press�o m�xima configurada

### P�gina 3: Offsets de Calibra��o
- Offset de temperatura
- Offset de umidade
- Offset de press�o

### P�gina 4: Conectividade Wi-Fi
- Status da conex�o
- Endere�o IP obtido
- Confirma��o de conectividade

---

## Sistema de Alertas

### Alertas Visuais
- **LED Verde**: Todos os sensores dentro dos limites normais
- **LED Vermelho**: Pelo menos um sensor acima do limite m�ximo
- **Matriz de LEDs**: Exibe ciclicamente as letras T, U, P com cores:
  - Verde: valor normal
  - Vermelho: valor acima do limite

### Alertas Sonoros
- **Buzzer**: Som intermitente quando qualquer sensor ultrapassa o limite m�ximo
- Padr�o: 200?s ligado, 50?s desligado

---

## Como Funciona

1. O programa inicializa dois barramentos I2C separados (sensores e display)
2. Configura Wi-Fi e estabelece conex�o com a rede configurada
3. Inicializa e l� periodicamente os sensores AHT20 e BMP280
4. Calcula a temperatura m�dia entre os dois sensores
5. Aplica offsets de calibra��o configur�veis
6. Verifica se os valores est�o dentro dos limites estabelecidos
7. Ativa alertas visuais e sonoros conforme necess�rio
8. Atualiza o display com as informa��es organizadas em p�ginas
9. Disponibiliza dados via servidor HTTP para acesso remoto
10. Exibe valores no terminal serial para debug

---

## Configura��o Wi-Fi

Defina as credenciais da rede Wi-Fi no c�digo:

```c
#define WIFI_SSID "SuaRede"
#define WIFI_PASS "SuaSenha"
```

---

## Depend�ncias e Compila��o

- SDK do Raspberry Pi Pico
    [https://github.com/raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk)
- Bibliotecas customizadas (devem estar no projeto):
    - `aht20.h` - Driver do sensor AHT20
    - `bmp280.h` - Driver do sensor BMP280
    - `ssd1306.h` - Driver do display OLED
    - `font.h` - Fontes para o display
    - `animacoes_led.h` - Controle da matriz de LEDs
    - Bibliotecas Wi-Fi do Pico W

**Compile com a extens�o do Raspberry Pi Pico no VS Code ou CMake.**

---

## Exemplo de Uso

1. Configure as credenciais Wi-Fi no c�digo
2. Compile e programe o Raspberry Pi Pico W com o firmware
3. Conecte os sensores, display e componentes conforme a tabela de pinos
4. Alimente o sistema (5V recomendado para a matriz de LEDs)
5. Abra um terminal serial (115200 baud) para acompanhar os dados
6. Use o bot�o A para navegar entre as p�ginas do display
7. Acesse o endere�o IP mostrado no display para interface web
8. O sistema iniciar� o monitoramento autom�tico com alertas

---

## Configura��es Personaliz�veis

### Limites de Alerta (edit�veis via interface web)
- `temp_max`: Temperatura m�xima (�C)
- `umid_max`: Umidade m�xima (%)
- `pres_max`: Press�o m�xima (Pa)

### Offsets de Calibra��o
- `temp_offset`: Corre��o da temperatura (�C)
- `umid_offset`: Corre��o da umidade (%)
- `pres_offset`: Corre��o da press�o (Pa)

---

## Observa��es

- O bot�o A permite navegar entre as 4 p�ginas do display
- O c�lculo da altitude considera press�o ao n�vel do mar de 101325 Pa
- A temperatura exibida � a m�dia entre AHT20 e BMP280 mais o offset
- Os alertas s�o ativados quando qualquer grandeza supera seu limite m�ximo
- A matriz de LEDs utiliza o protocolo WS2812B (LEDs endere��veis)
- O servidor HTTP permite monitoramento e configura��o remota
- Todos os valores s�o atualizados a cada 500ms
- O sistema mant�m conectividade Wi-Fi automaticamente

---

## Interface Web

Ap�s conectar ao Wi-Fi, acesse o IP mostrado no display para:
- Visualizar dados em tempo real
- Configurar limites de alerta
- Ajustar offsets de calibra��o
- Monitorar status do sistema remotamente

---

## Troubleshooting

- **Falha Wi-Fi**: Verifique credenciais e sinal da rede
- **Sensores n�o respondem**: Confirme conex�es I2C e endere�os
- **Display em branco**: Verifique alimenta��o e barramento I2C1
- **LEDs n�o acendem**: Confirme alimenta��o 5V na matriz de LEDs
- **Sem alertas**: Verifique se os limites est�o configurados corretamente