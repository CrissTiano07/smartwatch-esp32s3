/*
 * bsp_i2c.c — implementação do barramento I2C compartilhado.
 *
 * Usa o driver novo "i2c_master" do ESP-IDF 5.x. Ele já tem uma trava
 * (mutex) interna por barramento, então duas tasks podem chamar estas
 * funções sem uma atropelar a transação da outra.
 */
#include "bsp_i2c.h"
#include "esp_log.h"

static const char *TAG = "bsp_i2c";

// Pinos e velocidade (docs/HARDWARE.md).
#define BSP_I2C_SCL_GPIO   14
#define BSP_I2C_SDA_GPIO   15
#define BSP_I2C_FREQ_HZ    400000   // 400 kHz: "fast mode", todos os chips da placa suportam
#define BSP_I2C_TIMEOUT_MS 50       // tempo máximo esperando um chip; evita travar para sempre

// O único handle do barramento. 'static' = invisível fora deste arquivo,
// assim ninguém mais consegue mexer no barramento por fora do módulo.
static i2c_master_bus_handle_t s_bus = NULL;

// Tabela de chips conhecidos, só para deixar o scan legível.
typedef struct {
    uint8_t addr;
    const char *nome;
} chip_conhecido_t;

static const chip_conhecido_t s_conhecidos[] = {
    { 0x18, "ES8311 (codec de áudio)" },
    { 0x34, "AXP2101 (energia)" },
    { 0x38, "FT3168 (touch)" },
    { 0x40, "ES7210 (ADC microfones)" },
    { 0x51, "PCF85063 (RTC)" },
    { 0x6A, "QMI8658 (IMU)" },
    { 0x6B, "QMI8658 (IMU)" },
};

static const char *nome_do_chip(uint8_t addr)
{
    // sizeof(tabela)/sizeof(item) = número de itens da tabela.
    for (size_t i = 0; i < sizeof(s_conhecidos) / sizeof(s_conhecidos[0]); i++) {
        if (s_conhecidos[i].addr == addr) {
            return s_conhecidos[i].nome;
        }
    }
    return "desconhecido";
}

esp_err_t bsp_i2c_init(void)
{
    if (s_bus != NULL) {
        return ESP_OK;  // já inicializado: não cria um segundo barramento
    }

    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = BSP_I2C_SCL_GPIO,
        .sda_io_num = BSP_I2C_SDA_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        // Filtro contra ruído: ignora pulsos menores que 7 ciclos de clock.
        .glitch_ignore_cnt = 7,
        // Pull-up interno é fraco (~45 kΩ); a placa já tem resistores externos.
        // Ligamos mesmo assim como reforço — não atrapalha.
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao criar barramento: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "barramento ok (SCL %d, SDA %d, %d kHz)",
             BSP_I2C_SCL_GPIO, BSP_I2C_SDA_GPIO, BSP_I2C_FREQ_HZ / 1000);
    return ESP_OK;
}

int bsp_i2c_scan(void)
{
    if (s_bus == NULL) {
        ESP_LOGE(TAG, "scan antes do init");
        return 0;
    }

    int achados = 0;
    // 0x00..0x07 e 0x78..0x7F são endereços reservados pelo padrão I2C.
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // probe: manda só o endereço e vê se alguém responde "ACK".
        if (i2c_master_probe(s_bus, addr, BSP_I2C_TIMEOUT_MS) == ESP_OK) {
            ESP_LOGI(TAG, "  0x%02X  %s", addr, nome_do_chip(addr));
            achados++;
        }
    }
    ESP_LOGI(TAG, "scan: %d chip(s) responderam", achados);
    return achados;
}

esp_err_t bsp_i2c_add_device(uint8_t addr, i2c_master_dev_handle_t *out)
{
    if (s_bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(s_bus, &dev_cfg, out);
}

esp_err_t bsp_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg,
                           uint8_t *buf, size_t len)
{
    // Uma transação só: escreve o número do registrador e, sem soltar o
    // barramento (repeated start), lê a resposta. Padrão da maioria dos chips.
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, BSP_I2C_TIMEOUT_MS);
}

esp_err_t bsp_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    // Escrita: primeiro byte = registrador, segundo = valor.
    uint8_t pacote[2] = { reg, val };
    return i2c_master_transmit(dev, pacote, sizeof(pacote), BSP_I2C_TIMEOUT_MS);
}
