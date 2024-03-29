#!/usr/bin/python3

import os
import sys, struct, socket, signal, threading
import time

HOST = "127.0.0.1"
PORT = 57581

class ClientThread(threading.Thread):
	def __init__(self, conn, addr):
		threading.Thread.__init__(self)
		threading.Thread.daemon = True
		self.conn = conn
		self.addr = addr

	def run(self):
		#print("== Avviata connesione con ", self.addr, "==")
		
		#Il primo messaggio specifica se quello che sto per ricevere è un dato o un "segnale"
		data = recv_all(self.conn, 1)
		type = data.decode("utf-8")
		if(type == "!"):
			os.kill(os.getpid(), signal.SIGINT)
			self.closeConnection()
			return

		#Ricevi la somma
		data = recv_all(self.conn, 8)
		sum = struct.unpack("!q", data)[0]

		#Ricevi il nome del file
		data = recv_all(self.conn, 255)
		filename = data.decode("utf-8")

		#Stampa risultato
		print(f"{sum :>10} {filename :^10}")

		self.closeConnection()
		return #teoricamente non serve, ma per leggibilità lo metto
	
	def closeConnection(self):
		self.conn.shutdown(socket.SHUT_RDWR)
		self.conn.close()

		#print("== Chiusa connesione con ", self.addr, "==\n")


serverSocket = None

def main(host = HOST, port = PORT):
	global serverSocket
	signal.signal(signal.SIGINT, closeServer)

	serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	with serverSocket as s:
		try:
			s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
			s.bind((host, port))
			s.listen(1)

			#print("In ascolto per clients su indirizzo", HOST, "e porta", PORT, "...\n")
			while True:
				conn, addr = s.accept()

				#print("Connesso", addr, "!!")
				t = ClientThread(conn, addr)
				t.start()
		except OSError:
			pass
		


def closeServer(_s, _f):
	#for s in clients:
	#	s.shutdown(socket.SHUT_RDWR)
	#	s.close()

	serverSocket.shutdown(socket.SHUT_RDWR)
	serverSocket.close()

def recv_all(conn,n):
	chunks = b''
	bytes_recd = 0
	while bytes_recd < n:
		chunk = conn.recv(min(n - bytes_recd, 1024))
		if len(chunk) == 0:
			raise RuntimeError("socket connection broken")
		chunks += chunk
		bytes_recd = bytes_recd + len(chunk)

	assert len(chunks) == n
	return chunks


main()
