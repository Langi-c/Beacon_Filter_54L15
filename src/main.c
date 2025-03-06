#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/conn.h>

#define DEVICE_NAME "L4S3C 8SL" // Nombre del dispositivo
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

uint8_t Update = 0;
uint8_t new_namespace_id0[6] = {0x53, 0x4C, 0x42, 0x03, 0x00, 0x00};
uint8_t new_namespace_id1[6] = {0x53, 0x4C, 0x42, 0x04, 0x00, 0x00};


// Datos Eddystone-UID (solo UID y Nombre)
static uint8_t beacon_data[] = {
    0xaa, 0xfe,        /* Eddystone UUID */
	0x00,              /* Eddystone-UID frame type */
	0x00,              /* Tx power */
    0x53, 0x4C, 0x42, 0x01, 0x00, 0x00, /* Namespace ID: 6 Bytes */
};

// Publicidad optimizada
static struct bt_data beacon_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN), // Nombre del dispositivo
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
    BT_DATA(BT_DATA_SVC_DATA16, beacon_data, sizeof(beacon_data)) // UID
};

// Función para actualizar el Namespace ID
static int update_namespace(void)
{
	/* Se actualiza el Namespace ID */
	if (Update == 0) {
		Update = 1;
		memcpy(&beacon_ad[2].data[7], new_namespace_id0, 6);
	} else {
		Update = 0;
		memcpy(&beacon_ad[2].data[7], new_namespace_id1, 6);
	}

   /* Detiene el escaneo antes de detener la publicidad */
   int err = bt_le_scan_stop();
   if (err) {
	   printk("Failed to stop scanning (err %d)\n", err);
	   return -1;
   }

   /* Detiene la publicidad antes de reiniciarla */
	err = bt_le_adv_stop();
   if (err) {
	   printk("Failed to stop advertising (err %d)\n", err);
	   return -1;
   }

	/* Inicia el advertising como beacon Eddystone-UID */
	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, beacon_ad,
							ARRAY_SIZE(beacon_ad), NULL, 0);
	if (err) {
		printk("Error al iniciar la actualización de la publicidad (err %d)\n", err);
		return -1;
	}

	return 0;
} 

/* Callback que se invoca cuando se ha inicializado Bluetooth */
static void bluetooth_ready(int err)
{
   char addr_s[BT_ADDR_LE_STR_LEN];
   bt_addr_le_t addr = {0};
   size_t count = 1;

   if (err) {
	   printk("Bluetooth init failed (err %d)\n", err);
	   return;
   }

   printk("Bluetooth inicializado\n");

   /* Inicia el advertising como beacon Eddystone-UID */
   err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, beacon_ad,
						   ARRAY_SIZE(beacon_ad), NULL, 0);
   if (err) {
	   printk("Advertising failed to start (err %d)\n", err);
	   return;
   }

   bt_id_get(&addr, &count);
   bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));
   printk("Beacon iniciado, anunciando como %s\n", addr_s);	
}


// Función principal	
void main(void) {
	int err;


	printk("Iniciando demostración combinada de Beacon y Escaneo\n");

	err = bt_enable(bluetooth_ready);
	if (err) {
		printk("Error al inicializar Bluetooth (err %d)\n", err);
	}

	while (1)
	{
		k_sleep(K_SECONDS(8));
		err = update_namespace();	
		if (err) {
			printk("Error al actualizar el Namespace ID\n");
		}
		printk("\n\n\n Data: %02X%02X%02X%02X%02X%02X%02X \n\n\n", 
				beacon_ad[2].data[5], beacon_ad[2].data[6], beacon_ad[2].data[7], beacon_ad[2].data[8], 
				beacon_ad[2].data[9], beacon_ad[2].data[10], beacon_ad[2].data[11]
				);
	}
}
