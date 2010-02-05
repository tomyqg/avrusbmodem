/*
    LUFA Powered Wireless 3G Modem Host
	
    Copyright (C) Mike Alexander, 2010.
     Copyright (C) Dean Camera, 2010.
*/

/*
  Copyright 2010  Mike Alexander (mike [at] mikealex [dot] com)
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this 
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in 
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting 
  documentation, and that the name of the author not be used in 
  advertising or publicity pertaining to distribution of the 
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#include "ConnectionManagement.h"

struct uip_conn* ThisConn;
struct timer     Periodic_Timer;
uint8_t          DialSteps = 0;
uip_ipaddr_t     RemoteIPAddress;

const char* DialCommands[] = 
{	
	"AT\r\n",
	"AT&F\r\n",
	"AT+CPMS=\"SM\",\"SM\",\"\"\r\n",
	"ATQ0 V1 E1 S0=0 &C1 &D2 +FCLASS=0\r\n",
	"AT+CGDCONT=1,\"IP\",\"3services\",,0,0\r\n",
	"ATDT*99#\r\n",
	"PPP"					// PPP is a special case to transition to next state
};

void ConnectionManagement_ManageConnectionState(void)
{
	switch (ConnectedState)
	{
		case CONNECTION_MANAGE_STATE_DialConnection:
			ConnectionManagement_DialConnection();
			break;
		case CONNECTION_MANAGE_STATE_DoPPPNegotiation:
			PPP_ManagePPPNegotiation();
			break;
		case CONNECTION_MANAGE_STATE_InitializeTCPStack:
			ConnectionManagement_InitializeTCPStack();
			break;
		case CONNECTION_MANAGE_STATE_ConnectToRemoteHost:
			ConnectionManagement_ConnectToRemoteHost();
			break;
		case CONNECTION_MANAGE_STATE_ManageTCPConnection:
			ConnectionManagement_TCPIPTask();
			break;
	}
}

void ConnectionManagement_DialConnection(void)
{
	char Command[64];

	if (USB_HostState == HOST_STATE_Configured)	
	{
		while (Modem_ReceiveBuffer.Elements)
		  Debug_PrintChar(Buffer_GetElement(&Modem_ReceiveBuffer));
			
		if (TIME > 100)
		{
			TIME = 0;
			strcpy(Command, DialCommands[DialSteps++]);

			if (strcmp(Command, "PPP") == 0)
			{
				Debug_Print("Starting PPP\r\n");
				DialSteps = 0;
				ConnectedState = CONNECTION_MANAGE_STATE_DoPPPNegotiation;
				return;
			}

			Debug_Print("Sending command: ");
			Debug_Print(Command);
			
			char* CommandPtr = Command;
			while (*CommandPtr)
			  Buffer_StoreElement(&Modem_SendBuffer, *(CommandPtr++));
		}
	}
}

void ConnectionManagement_InitializeTCPStack(void)
{
	Debug_Print("Initialise TCP Stack\r\n");

	// uIP component init
	network_init();
	clock_init();
	uip_init();

	// Periodic connection management timer init
	timer_set(&Periodic_Timer, CLOCK_SECOND / 2);

	// Set this machine's IP address
	uip_ipaddr_t LocalIPAddress;
	uip_ipaddr(&LocalIPAddress, IPAddr1, IPAddr2, IPAddr3, IPAddr4);
	uip_sethostaddr(&LocalIPAddress);

	// Set remote IP address
	uip_ipaddr(&RemoteIPAddress, 192, 0, 32, 10);	// www.example.com

	ConnectedState = CONNECTION_MANAGE_STATE_ConnectToRemoteHost;
	TIME = 2000;			// Make the first CONNECT happen straight away
}

void ConnectionManagement_ConnectToRemoteHost(void)
{
	if (TIME > 1000)		//Try to connect every 1 second
	{
		TIME = 0;
		
		// Connect to the remote machine
		ThisConn = uip_connect(&RemoteIPAddress, HTONS(80));

		if (ThisConn != NULL)
		{
			Debug_Print("Connected to host\r\n");
			ConnectedState = CONNECTION_MANAGE_STATE_ManageTCPConnection;
			TIME = 3001;			// Make the first GET happen straight away
		}
		else
		{
			Debug_Print("Failed to Connect\r\n");
		}

		Debug_Print("Maximum Segment Size: 0x"); Debug_PrintHex(uip_mss() / 256);
		Debug_Print("0x"); Debug_PrintHex(uip_mss() & 255); 
		Debug_Print("\r\n");
	}
}

void ConnectionManagement_TCPIPTask(void)
{
	int i, j;

	uip_len = network_read();

	if (uip_len == -1)								// Got a non-SLIP packet. Probably a LCP-TERM Re-establish link.
	{
		Debug_Print("Got non-PPP packet\r\n");
		TIME = 0;
		ConnectedState = CONNECTION_MANAGE_STATE_DialConnection;
		return;
	}

	if (uip_len > 0)								// We have some data to process
	{
	
		/********************** Debug **********************/

		Debug_Print("\r\nReceive:\r\n");
	
		for (i = 0; i < uip_len; i += 16)
		{	
			// Print the hex
			for (j = 0; j < 16; j++)
			{
				if ((i + j) >= uip_len)
					break;

				Debug_PrintHex(*(uip_buf + i + j));
			}
			
			Debug_Print("\r\n");	
			
			// Print the ASCII
			for (j = 0; j < 16; j++)
			{
				if ((i + j) >= uip_len)
					break;

				if (*(uip_buf + i + j) >= 0x20 && *(uip_buf + i + j) <= 0x7e)
				{
					Debug_PrintChar(' ');
					Debug_PrintChar(*(uip_buf + i + j));
					Debug_PrintChar(' ');
				}
				else
				{
					Debug_Print(" . ");
				}
			}

			Debug_Print("\r\n");
		}

		/********************** Debug **********************/

		uip_input();
 
	 	// If the above function invocation resulted in data that should be sent out on the network, the global variable uip_len is set to a value > 0.
	 	if (uip_len > 0)
		{
	 		network_send();
	 	}
	}
	else if (timer_expired(&Periodic_Timer))
	{
		timer_reset(&Periodic_Timer);

		for (int i = 0; i < UIP_CONNS; i++)
		{
	 		uip_periodic(i);
 
	 		// If the above function invocation resulted in data that should be sent out on the network, the global variable uip_len is set to a value > 0.
	 		if (uip_len > 0)
	 		{
	 			network_send();
	 		}
		}
	}

	char c;

	// Read any available data from the serial port.
	// If we see a '!' in the input stream, switch debug mode on. If we see a "@", switch debug mode off.
	while ((c = getchar()) != EOF)
	{
		if (c == '!')
		{
			puts("\r\nDebug on\r\n");
			DebugModeEnabled = true;
		}
		else if (c == '@')
		{
			puts("\r\nDebug off\r\n");
			DebugModeEnabled = false;
		}

		c = getchar();
	}	
}

void TCPCallback(void)
{
	Debug_PrintChar('*');

	if (uip_newdata())
		Debug_Print("NewData ");

	if (uip_acked())
		Debug_Print("Acked ");
	
	if (uip_connected())
		Debug_Print("Connected ");

	if (uip_closed())
	{
		Debug_Print("Closed - Reconnecting...");
		_delay_ms(1000);
		ConnectedState = CONNECTION_MANAGE_STATE_ConnectToRemoteHost;
	}

	if (uip_aborted())
	{
		Debug_Print("Aborted - Reconnecting... ");
		_delay_ms(1000);
		ConnectedState = CONNECTION_MANAGE_STATE_ConnectToRemoteHost;
	}

	if (uip_timedout())
	{
		Debug_Print("Timeout - Reconnecting...");
		uip_abort();
		_delay_ms(1000);
		ConnectedState = CONNECTION_MANAGE_STATE_ConnectToRemoteHost;
	}

	if (uip_poll() && TIME > 3000)
	{
		TIME = 0;
		
		Debug_Print("\r\nSending GET\r\n");
		SendGET();
	}
	
	if (uip_rexmit())
	{
		Debug_Print("\r\nRetransmit GET\r\n");
		SendGET();
	}

	if (uip_newdata())
	{
		device_enqueue(uip_appdata, uip_datalen());
		
		if (device_queue_full())
		{
			uip_stop();
		}
	}

	if (uip_poll() && uip_stopped(ThisConn))
	{
		if (!device_queue_full())
		{
			uip_restart();
		}
	}
}

void SendGET(void)
{
	uip_send("GET / HTTP/1.1\r\nHost: www.example.com\r\nConnection: Keep-Alive\r\n\r\n", 65);
}
