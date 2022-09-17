/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include "quakedef.h"
#include "net_loop.h"

#define PARANOID

bool localconnectpending = false;
qsocket_t *loop_client = NULL;
qsocket_t *loop_server = NULL;

int Loop_Init(void)
{
	if (cls.state == ca_dedicated)
		return -1;
	return 0;
}

void Loop_Listen(bool state)
{
}

void Loop_SearchForHosts(bool xmit)
{
	if (!sv.active)
		return;

	hostCacheCount = 1;
	if (strcmp(hostname.string, "UNNAMED") == 0)
		strcpy(hostcache[0].name, "local");
	else
		strcpy(hostcache[0].name, hostname.string);
	strcpy(hostcache[0].map, sv.name);
	hostcache[0].users = net_activeconnections;
	hostcache[0].maxusers = svs.maxclients;
	hostcache[0].driver = net_driverlevel;
	strcpy(hostcache[0].cname, "local");
}

qsocket_t *Loop_Connect(const char *host)
{
	if (strcmp(host, "local"))
		return NULL;

	localconnectpending = true;

	if (!loop_client)
	{
		if (!(loop_client = NET_NewQSocket()))
		{
			Con_Printf("Loop_Connect: no qsocket available\n");
			return NULL;
		}
		strcpy(loop_client->address, "localhost");
	}
	loop_client->receiveMessageLength = 0;
	loop_client->sendMessageLength = 0;
	loop_client->canSend = true;
	loop_client->mod = MOD_PROQUAKE; // JPG - added this
	loop_client->client_port = 0;

	if (!loop_server)
	{
		if (!(loop_server = NET_NewQSocket()))
		{
			Con_Printf("Loop_Connect: no qsocket available\n");
			return NULL;
		}
		strcpy(loop_server->address, "LOCAL");
	}
	loop_server->receiveMessageLength = 0;
	loop_server->sendMessageLength = 0;
	loop_server->canSend = true;
	loop_server->mod = MOD_PROQUAKE; // JPG - added this
	loop_server->client_port = 0;

	loop_client->driverdata = (void *) loop_server;
	loop_server->driverdata = (void *) loop_client;

	return loop_client;
}

qsocket_t *Loop_CheckNewConnections(void)
{
	if (!localconnectpending)
		return NULL;

	localconnectpending = false;
	loop_server->sendMessageLength = 0;
	loop_server->receiveMessageLength = 0;
	loop_server->canSend = true;
	loop_client->sendMessageLength = 0;
	loop_client->receiveMessageLength = 0;
	loop_client->canSend = true;
	return loop_server;
}

static int IntAlign(int value)
{
	return (value + (sizeof(int) - 1)) & (~(sizeof(int) - 1));
}

int Loop_GetMessage(qsocket_t *sock)
{
	if (sock->receiveMessageLength == 0)
		return 0;

#ifdef PARANOID
	// need to at least have the 4-byte message header
	if (sock->receiveMessageLength < 4)
		Sys_Error("short receiveMessageLength");
#endif

	int type = (int)sock->receiveMessage[0];

	size_t length = (size_t)((sock->receiveMessage[1] << 0) |
			         (sock->receiveMessage[2] << 8));

#ifdef PARANOID
	if (length + 4 > (size_t)sock->receiveMessageLength)
		Sys_Error("long message");
#endif

	// alignment byte skipped here
	SZ_Clear(&net_message);
	SZ_Write(&net_message, &sock->receiveMessage[4], length);

	size_t full_length = IntAlign(length + 4);

#ifdef PARANOID
	if (full_length > (size_t)sock->receiveMessageLength)
		Sys_Error("alignment long message");
#endif

	sock->receiveMessageLength -= full_length;
	memcpy(sock->receiveMessage, &sock->receiveMessage[full_length], sock->receiveMessageLength);

	qsocket_t *other_side = (qsocket_t *)sock->driverdata;
	if (other_side && type == 1)
		other_side->canSend = true;

	return type;
}

int Loop_SendMessage(qsocket_t *sock, sizebuf_t *data)
{
	qsocket_t *other_side = (qsocket_t *)sock->driverdata;
	if (!other_side)
		return -1;

	int current_size = other_side->receiveMessageLength;
	if ((current_size + 4 + data->cursize + 4) > NET_MAXMESSAGE)
		Sys_Error("overflow\n");

	// directly write into other side message buffer
	byte *buffer = other_side->receiveMessage + current_size;

	// message type
	*buffer++ = 1;

	// length
	*buffer++ = (byte)((data->cursize >> 0) & 0xff);
	*buffer++ = (byte)((data->cursize >> 8) & 0xff);

	// align
	buffer++;

	// message
	memcpy(buffer, data->data, data->cursize);

	other_side->receiveMessageLength += IntAlign(data->cursize + 4);

	sock->canSend = false;

	return 1;
}

int Loop_SendUnreliableMessage(qsocket_t *sock, sizebuf_t *data)
{
	qsocket_t *other_side = (qsocket_t *)sock->driverdata;
	if (!other_side)
		return -1;

	int current_size = other_side->receiveMessageLength;
	if ((current_size + 4 + data->cursize + 4) > NET_MAXMESSAGE)
		return 0;

	// directly write into other side message buffer
	byte *buffer = other_side->receiveMessage + current_size;

	// message type
	*buffer++ = 1;

	// length
	*buffer++ = (byte)((data->cursize >> 0) & 0xff);
	*buffer++ = (byte)((data->cursize >> 8) & 0xff);

	// align
	buffer++;

	// message
	memcpy(buffer, data->data, data->cursize);

	other_side->receiveMessageLength += IntAlign(data->cursize + 4);

	return 1;
}

bool Loop_CanSendMessage(qsocket_t *sock)
{
	if (!sock->driverdata)
		return false;
	return sock->canSend;
}

bool Loop_CanSendUnreliableMessage(qsocket_t *sock)
{
	return true;
}

void Loop_Close(qsocket_t *sock)
{
	qsocket_t *other_side = (qsocket_t *)sock->driverdata;
	if (other_side)
		other_side->driverdata = NULL;

	sock->receiveMessageLength = 0;
	sock->sendMessageLength = 0;
	sock->canSend = true;
	if (sock == loop_client)
		loop_client = NULL;
	else
		loop_server = NULL;
}

void Loop_Shutdown(void)
{
}
