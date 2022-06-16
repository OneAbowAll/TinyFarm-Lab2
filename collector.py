from fileinput import close
from os import getpid, kill
import sys, struct, socket, signal, threading

HOST = "127.0.0.1"
PORT = 65434

class ClientThread(threading.Thread):
	def __init__(self, conn, addr):
		threading.Thread.__init__(self)
		threading.Thread.deamon = True
		self.conn = conn
		self.addr = addr

	def run(self):
		#print("== Avviata connesione con ", self.addr, "==")
		try:
			with self.conn:
				#Il primo messaggio specifica se quello che sto per ricevere è un dato o un "segnale"
				data = recv_all(self.conn, 1)
				type = data.decode("utf-8")
				if(type == "!"):
					kill(getpid(), signal.SIGINT)

				#Ricevi la somma
				data = recv_all(self.conn, 8)
				sum = struct.unpack("!q", data)[0]

				#Ricevi il nome del file
				data = recv_all(self.conn, 255)
				filename = data.decode("utf-8")

				#Stampa risultato
				print(f"{sum :>10} {filename :^10}")
		except RuntimeError:
			pass
		except OSError:
			pass

		self.conn.close()
		#print("== Chiusa connesione con ", self.addr, "==\n")

serverSocket = None
clients = []

def main(host = HOST, port = PORT):
	global serverSocket
	signal.signal(signal.SIGINT, closeServer)
	
	serverSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	with serverSocket as s:
		try:
			s.bind((host, port))
			s.listen(1)

			print("In ascolto per clients su indirizzo", HOST, "e porta", PORT, "...\n")
			while True:
				conn, addr = s.accept()

				#print("Connesso", addr, "!!")
				t = ClientThread(conn, addr)
				t.start()

				clients.append(conn)
		except OSError:
			pass

def closeServer(s, f):
	print("\nChiusura in corso...")

	for s in clients:
		s.close()

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

#Se non vengono specificate porte o indirizzi usare quelli default
#Questa roba non serve ma la tengo per sperimentare dopo :)
if len(sys.argv) == 1:
        main()
elif len(sys.argv)==2:
  main(sys.argv[1])
elif len(sys.argv)==3:
  main(sys.argv[1], int(sys.argv[2]))
