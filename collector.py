import sys, struct, socket, threading

HOST = "127.0.0.1"
PORT = 65432

class ClientThread(threading.Thread):
	def __init__(self, conn, addr):
		threading.Thread.__init__(self)
		self.conn = conn
		self.addr = addr
		print("Per curiosità ", self.conn, " ", self.addr, " ", self.ident)

	def run(self):
		print("== Avviata connesione con ", self.addr, " ==")
		sleep(10)
		print("== Chiusa connesione con ", self.addr, " ==")


def main(host = HOST, port = PORT):
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
		try:
			s.bind((host, port))
			s.listen()
			while True:
				print("In ascolto per clients su indirizzo", HOST, "e porta", PORT, "...")
				
				conn, addr = s.accept()
				t = ClientThread(conn, addr)
				t.start()
				
		except KeyboardInterrupt:
			pass

	print("Chiusura in corso...")
	s.shutdown(socket.SHUT_RDWR)


main()