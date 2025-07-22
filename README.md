# Estação Meteorológica Completa com Raspberry Pi Pico

## Por: Mateus Moreira da Silva

Este projeto implementa uma estação meteorológica completa utilizando o microcontrolador **RP2040** (Raspberry Pi Pico), sensores digitais de temperatura/umidade (**AHT20**), pressão/temperatura (**BMP280**), display OLED I2C (**SSD1306**), matriz de LEDs, buzzer, LEDs RGB e conectividade Wi-Fi.

## Funcionalidades

- Leitura simultânea de temperatura e umidade (AHT20)
- Leitura de pressão, temperatura e cálculo de altitude (BMP280)
- Cálculo de temperatura média entre os dois sensores com calibração por offset
- Exibição das informações em tempo real no display OLED SSD1306 com 4 páginas navegáveis
- Sistema de alertas visuais e sonoros quando os sensores ultrapassam limites máximos:
  - LEDs RGB (verde = normal, vermelho = alerta)
  - Buzzer com som intermitente
  - Matriz de LEDs 5x5 mostrando status das grandezas (T, U, P)
- Conectividade Wi-Fi com servidor HTTP para acesso remoto
- Comunicação via barramento I2C com múltiplos dispositivos
- Saída dos dados para o terminal serial (debug/log)
- Navegação entre páginas do display com botão A
- Interface web para monitoramento remoto

---

## Hardware Necessário

- Raspberry Pi Pico W (RP2040 com Wi-Fi)
- Sensor AHT20 ? I2C (temperatura e umidade)
- Sensor BMP280 ? I2C (pressão e temperatura)
- Display OLED SSD1306 (128x64) ? I2C
- Matriz de LEDs RGB 5x5 endereçável (WS2812B)
- Buzzer ativo
- 2x LEDs (verde e vermelho)
- Botão para navegação
- Resistores de pull-up
- Cabos de conexão

---

## Conexões (GPIO)

| Dispositivo              | Barramento | SDA/GPIO | SCL/GPIO | Endereço I2C padrão           |
|--------------------------|------------|----------|----------|-------------------------------|
| AHT20 + BMP280           | i2c0       | 4        | 5        | 0x38 (AHT20), 0x76 (BMP280) |
| SSD1306 Display          | i2c1       | 14       | 15       | 0x3C                         |
| Matriz LEDs (WS2812B)    | -          | 7        | -        | -                            |
| LED Verde                | -          | 11       | -        | -                            |
| LED Vermelho             | -          | 12       | -        | -                            |
| Buzzer                   | -          | 21       | -        | -                            |
| Botão A (Navegação)      | -          | 5        | -        | -                            |

---

## Páginas do Display

O display possui 4 páginas navegáveis através do botão A:

### Página 1: Dados Principais
- Temperatura média (°C)
- Umidade (%)
- Pressão (kPa)

### Página 2: Limites Máximos
- Temperatura máxima configurada
- Umidade máxima configurada
- Pressão máxima configurada

### Página 3: Offsets de Calibração
- Offset de temperatura
- Offset de umidade
- Offset de pressão

### Página 4: Conectividade Wi-Fi
- Status da conexão
- Endereço IP obtido
- Confirmação de conectividade

---

## Sistema de Alertas

### Alertas Visuais
- **LED Verde**: Todos os sensores dentro dos limites normais
- **LED Vermelho**: Pelo menos um sensor acima do limite máximo
- **Matriz de LEDs**: Exibe ciclicamente as letras T, U, P com cores:
  - Verde: valor normal
  - Vermelho: valor acima do limite

### Alertas Sonoros
- **Buzzer**: Som intermitente quando qualquer sensor ultrapassa o limite máximo
- Padrão: 200?s ligado, 50?s desligado

---

## Como Funciona

1. O programa inicializa dois barramentos I2C separados (sensores e display)
2. Configura Wi-Fi e estabelece conexão com a rede configurada
3. Inicializa e lê periodicamente os sensores AHT20 e BMP280
4. Calcula a temperatura média entre os dois sensores
5. Aplica offsets de calibração configuráveis
6. Verifica se os valores estão dentro dos limites estabelecidos
7. Ativa alertas visuais e sonoros conforme necessário
8. Atualiza o display com as informações organizadas em páginas
9. Disponibiliza dados via servidor HTTP para acesso remoto
10. Exibe valores no terminal serial para debug

---

## Configuração Wi-Fi

Defina as credenciais da rede Wi-Fi no código:

```c
#define WIFI_SSID "SuaRede"
#define WIFI_PASS "SuaSenha"
```

---

## Dependências e Compilação

- SDK do Raspberry Pi Pico
    [https://github.com/raspberrypi/pico-sdk](https://github.com/raspberrypi/pico-sdk)
- Bibliotecas customizadas (devem estar no projeto):
    - `aht20.h` - Driver do sensor AHT20
    - `bmp280.h` - Driver do sensor BMP280
    - `ssd1306.h` - Driver do display OLED
    - `font.h` - Fontes para o display
    - `animacoes_led.h` - Controle da matriz de LEDs
    - Bibliotecas Wi-Fi do Pico W

**Compile com a extensão do Raspberry Pi Pico no VS Code ou CMake.**

---

## Exemplo de Uso

1. Configure as credenciais Wi-Fi no código
2. Compile e programe o Raspberry Pi Pico W com o firmware
3. Conecte os sensores, display e componentes conforme a tabela de pinos
4. Alimente o sistema (5V recomendado para a matriz de LEDs)
5. Abra um terminal serial (115200 baud) para acompanhar os dados
6. Use o botão A para navegar entre as páginas do display
7. Acesse o endereço IP mostrado no display para interface web
8. O sistema iniciará o monitoramento automático com alertas

---

## Configurações Personalizáveis

### Limites de Alerta (editáveis via interface web)
- `temp_max`: Temperatura máxima (°C)
- `umid_max`: Umidade máxima (%)
- `pres_max`: Pressão máxima (Pa)

### Offsets de Calibração
- `temp_offset`: Correção da temperatura (°C)
- `umid_offset`: Correção da umidade (%)
- `pres_offset`: Correção da pressão (Pa)

---

## Observações

- O botão A permite navegar entre as 4 páginas do display
- O cálculo da altitude considera pressão ao nível do mar de 101325 Pa
- A temperatura exibida é a média entre AHT20 e BMP280 mais o offset
- Os alertas são ativados quando qualquer grandeza supera seu limite máximo
- A matriz de LEDs utiliza o protocolo WS2812B (LEDs endereçáveis)
- O servidor HTTP permite monitoramento e configuração remota
- Todos os valores são atualizados a cada 500ms
- O sistema mantém conectividade Wi-Fi automaticamente

---

## Interface Web

Após conectar ao Wi-Fi, acesse o IP mostrado no display para:
- Visualizar dados em tempo real
- Configurar limites de alerta
- Ajustar offsets de calibração
- Monitorar status do sistema remotamente

---

Link do GitHub: https://github.com/Mateus-MDS/Estacao_de_Medicao_Conectada.git
Video do Youtube: https://youtu.be/U-QRE9vPvSU
