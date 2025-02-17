/* main.c - Application main entry point */
/*
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <zephyr/types.h>
 #include <stddef.h>
 #include <errno.h>
 #include <zephyr/kernel.h>
 #include <zephyr/sys/printk.h>
 
 #include <zephyr/bluetooth/bluetooth.h>
 #include <zephyr/bluetooth/hci.h>
 #include <zephyr/bluetooth/conn.h>
 #include <zephyr/bluetooth/uuid.h>
 #include <zephyr/bluetooth/gatt.h>
 #include <zephyr/sys/byteorder.h>
 
 /* Declaración de la función que parsea los datos de publicidad */
 static void parse_advertising_data(const struct net_buf_simple *ad_buf);
 
 /* Callback que se invoca cuando se encuentra un dispositivo durante el escaneo */
 static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			  struct net_buf_simple *ad)
 {
	 char addr_str[BT_ADDR_LE_STR_LEN];
 
	 /* Convertimos la dirección a string para imprimirla */
	 bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
 
	 /* Imprimimos algo de información básica del dispositivo */
	 printk("[DEVICE]: %s, AD evt type %u, RSSI %i dBm\n", addr_str, type, rssi);
 
	 /*
	  * Eddystone (UID, URL, TLM) a menudo se anuncia en paquetes
	  * no conectables (por ejemplo, ADV_NONCONN_IND).
	  * Por ello, NO descartamos los paquetes en base a 'type'.
	  */
 
	 /* Llamamos a la función que recorrerá los campos AD */
	 parse_advertising_data(ad);
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
 static void parse_advertising_data(const struct net_buf_simple *ad_buf)
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
 
 int main(void)
 {
	 int err;
 
	 err = bt_enable(NULL);
	 if (err) {
		 printk("Bluetooth init falló (err %d)\n", err);
		 return 0;
	 }
 
	 printk("Bluetooth inicializado\n");
 
	 start_scan();
 
	 return 0;
 }
 