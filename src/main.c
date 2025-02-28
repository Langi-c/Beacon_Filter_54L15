/* main.c - Application main entry point */
/*
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <stdio.h>
 #include <string.h>

 #include <zephyr/types.h>
 #include <stddef.h>
 #include <errno.h>
 #include <zephyr/kernel.h>
 #include <zephyr/sys/printk.h>
 #include <zephyr/sys/util.h>
 
 #include <zephyr/bluetooth/bluetooth.h>
 #include <zephyr/bluetooth/hci.h>
 #include <zephyr/bluetooth/conn.h>
 #include <zephyr/bluetooth/uuid.h>
 #include <zephyr/bluetooth/gatt.h>
 #include <zephyr/sys/byteorder.h>
 
/* --- Beacon Advertisement Configuration --- */
#define DEVICE_NAME         CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN     (sizeof(DEVICE_NAME) - 1)


uint8_t Update = 0;
uint8_t new_namespace_id0[10] = {0x53, 0x4C, 0x42, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t new_namespace_id1[10] = {0x53, 0x4C, 0x42, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


static const struct bt_data beacon_ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
	              0xaa, 0xfe,         /* Eddystone UUID */
	              0x00,               /* Eddystone-UID frame type */
	              0x00,               /* Calibrated Tx power at 0m */
	              0x53, 0x4C, 0x42, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Namespace ID */
	              0x01, 0x02, 0x03, 0x04, 0x05, 0x06, /* Instance ID */
	              0x00, 0x00)        /* RFU */
};

static const struct bt_data beacon_sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};


 /* Dirección MAC del dispositivo a buscar */
static bt_addr_le_t target_addr;

 /* Declaración de la función que parsea los datos de publicidad */
 static void parse_advertising_data(const struct net_buf_simple *ad_buf, 
									const char *addr_str, 
									uint8_t type, 
									int8_t rssi);
 
 /* Callback que se invoca cuando se encuentra un dispositivo durante el escaneo */
 static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			  struct net_buf_simple *ad)
 {
	 char addr_str[BT_ADDR_LE_STR_LEN];
	 
	 char target_addr_str[BT_ADDR_LE_STR_LEN];
  
	 target_addr.type = BT_ADDR_LE_PUBLIC;
	 bt_addr_le_from_str("4C:54:53:F3:10:01", "public", &target_addr);		// Dirección MAC del dispositivo a buscar

	 /* Convertimos la dirección a string para imprimirla */
	 bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	 bt_addr_le_to_str(&target_addr, target_addr_str, sizeof(target_addr_str));

	 if (strcmp(addr_str, target_addr_str) == 0) {
		parse_advertising_data(ad, addr_str, type, rssi);
	 }
 }
 
 /* Función que inicia el escaneo */
 static void start_scan(void)
 {
	 int err;
 
	 /* Se usará escaneo pasivo */
	 err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	 if (err) {
		 printk("Fallo al iniciar el escaneo (err %d)\n", err);
		 return;
	 }
 
	 printk("Escaneo iniciado (modo pasivo)\n");
 }
 
 /* Función para analizar cada campo AD y buscar Eddystone-UID */
 static void parse_advertising_data(const struct net_buf_simple *ad_buf, const char *addr_str, uint8_t type, int8_t rssi)
 {
	 /* Creamos una copia local de 'ad_buf' para poder avanzar con net_buf_simple_* */
	 struct net_buf_simple ad = *ad_buf;
 
	 while (ad.len > 1) {
		 /* El primer byte indica la longitud del campo (sin incluir este byte) */
		 uint8_t length = net_buf_simple_pull_u8(&ad);
		 if (length == 0 || length > ad.len) {
			 return;
		 }
 
		 /* El segundo byte es el "AD type" */
		 uint8_t ad_type = net_buf_simple_pull_u8(&ad);
		 length--;
 
		 switch (ad_type) {
		 case BT_DATA_SVC_DATA16: {
			 /*
			  * Los primeros 2 bytes son el UUID en little-endian.
			  */
			 if (length < 2) {
				 /* No hay suficientes bytes para el UUID */
				 net_buf_simple_pull(&ad, length);
				 break;
			 }
 
			 uint16_t uuid = net_buf_simple_pull_le16(&ad);
			 length -= 2;
 
			 /* Verificamos si es Eddystone (0xFEAA) */
			 if (uuid == 0xFEAA && length > 0) {
				 uint8_t frame_type = net_buf_simple_pull_u8(&ad);
				 length--;
 
				 if (frame_type == 0x00 && length >= 1 + 10) {
					 int8_t tx_power = (int8_t)net_buf_simple_pull_u8(&ad);
					 length--;
 
					 /* Leemos los 10 bytes del Namespace */
					 uint8_t namespace_id[10];
					 for (int i = 0; i < 10; i++) {
						 namespace_id[i] = net_buf_simple_pull_u8(&ad);
					 }
					 length -= 10;
					
					 printk("[DEVICE]: %s, AD evt type %u, RSSI %i dBm\n", addr_str, type, rssi);
					 printk("  >> Eddystone-UID detectado\n");
					 printk("     Tx Power: %d dBm\n", tx_power);
 
					 printk("     Namespace: ");
					 for (int i = 0; i < 10; i++) {
						 printk("%02X", namespace_id[i]);
					 }
					 printk("\n");
				 } else {
					 /* No es UID o no hay bytes suficientes */
					 net_buf_simple_pull(&ad, length);
				 }
			 } else {
				 /* No es 0xFEAA o no hay más bytes */
				 net_buf_simple_pull(&ad, length);
			 }
		 } break;
 
		 default:
			 net_buf_simple_pull(&ad, length);
			 break;
		 }
	 }
 }
 
 
 static int update_namespace(void)
 {
	 /* Se actualiza el Namespace ID */
	 if (Update == 0) {
		 Update = 1;
		 memcpy(&beacon_ad[2].data[7], new_namespace_id0, 10);
	 } else {
		 Update = 0;
		 memcpy(&beacon_ad[2].data[7], new_namespace_id1, 10);
	 }

    /* Detiene la publicidad antes de reiniciarla */
    int err = bt_le_adv_stop();
    if (err) {
        printk("Failed to stop advertising (err %d)\n", err);
        return -1;
    }

	 /* Inicia el advertising como beacon Eddystone-UID */
	 err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, beacon_ad,
							 ARRAY_SIZE(beacon_ad), beacon_sd,
							 ARRAY_SIZE(beacon_sd));
	 if (err) {
		 printk("Advertising Update failed to start (err %d)\n", err);
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

	printk("Bluetooth initialized\n");

	/* Inicia el advertising como beacon Eddystone-UID */
	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, beacon_ad,
	                        ARRAY_SIZE(beacon_ad), beacon_sd,
	                        ARRAY_SIZE(beacon_sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));
	printk("Beacon started, advertising as %s\n", addr_s);	

	/* Inicia el escaneo para buscar el dispositivo objetivo */
	start_scan();
}
 
 int main(void)
 {
	 int err;


	 printk("Starting Combined Beacon and Scan Demo\n");

	 err = bt_enable(bluetooth_ready);
	 if (err) {
		 printk("Bluetooth init failed (err %d)\n", err);
	 }
 
	 while (1)
	 {
		 k_sleep(K_SECONDS(8));
		 err = update_namespace();	
		 if (err) {
			 printk("Error al actualizar el Namespace ID\n");
		 }
		 printk("Namespace ID actualizado: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			 beacon_ad[2].data[7], beacon_ad[2].data[8], beacon_ad[2].data[9],
			 beacon_ad[2].data[10], beacon_ad[2].data[11], beacon_ad[2].data[12],
			 beacon_ad[2].data[13], beacon_ad[2].data[14], beacon_ad[2].data[15],
			 beacon_ad[2].data[16]);
	 }

	 return 0;
 }
 