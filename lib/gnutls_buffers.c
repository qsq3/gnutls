/*
 *      Copyright (C) 2000,2001 Nikos Mavroyanopoulos
 *
 * This file is part of GNUTLS.
 *
 * GNUTLS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GNUTLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <gnutls_int.h>
#include <gnutls_errors.h>
#include <gnutls_num.h>
#include <gnutls_record.h>
#include <gnutls_buffers.h>

/* This is the only file that uses the berkeley sockets API.
 * 
 * Also holds all the buffering code used in gnutls.
 */

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#ifndef EAGAIN
# define EAGAIN EWOULDBLOCK
#endif

inline 
static int RET( int err) {
	if (err==EAGAIN) return GNUTLS_E_AGAIN;
	return GNUTLS_E_INTERRUPTED;
}

#ifdef IO_DEBUG
# include <io_debug.h>
#endif

/* Buffers received packets of type APPLICATION DATA and
 * HANDSHAKE DATA.
 */
int gnutls_insert_to_data_buffer(ContentType type, GNUTLS_STATE state, char *data, int length)
{
	int old_buffer;

	if (length==0) return 0;
	switch( type) {
	case GNUTLS_APPLICATION_DATA:
		old_buffer = state->gnutls_internals.buffer.size;

		state->gnutls_internals.buffer.size += length;
#ifdef BUFFERS_DEBUG
	_gnutls_log( "RECORD BUFFER: Inserted %d bytes of Data(%d)\n", length, type);
#endif
		state->gnutls_internals.buffer.data =
		    gnutls_realloc_fast(state->gnutls_internals.buffer.data,
			   state->gnutls_internals.buffer.size);
		memcpy(&state->gnutls_internals.buffer.data[old_buffer], data, length);
		break;
	case GNUTLS_HANDSHAKE:
		old_buffer = state->gnutls_internals.buffer_handshake.size;

		state->gnutls_internals.buffer_handshake.size += length;
#ifdef BUFFERS_DEBUG
	_gnutls_log( "HANDSHAKE BUFFER: Inserted %d bytes of Data(%d)\n", length, type);
#endif
		state->gnutls_internals.buffer_handshake.data =
		    gnutls_realloc_fast(state->gnutls_internals.buffer_handshake.data,
			   state->gnutls_internals.buffer_handshake.size);
		memcpy(&state->gnutls_internals.buffer_handshake.data[old_buffer], data, length);
		break;
	
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_PARAMETERS;
	}

	return 0;

}

int gnutls_get_data_buffer_size(ContentType type, GNUTLS_STATE state)
{
	switch( type) {
		case GNUTLS_APPLICATION_DATA:
			return state->gnutls_internals.buffer.size;
	
		case GNUTLS_HANDSHAKE:
			return state->gnutls_internals.buffer_handshake.size;
		
		default:
			return GNUTLS_E_INVALID_PARAMETERS;
	}
	return 0;
}

/**
  * gnutls_check_pending - checks if there are any data to receive in gnutls buffers.
  * @state: is a &GNUTLS_STATE structure.
  *
  * This function checks if there are any data to receive
  * in the gnutls buffers. Returns the size of that data or 0.
  * Notice that you may also use select() to check for data in
  * the TCP connection, instead of this function.
  * (gnutls leaves some data in the tcp buffer in order for select
  * to work).
  **/
int gnutls_check_pending(GNUTLS_STATE state) {
	return gnutls_get_data_buffer_size(GNUTLS_APPLICATION_DATA, state);
}

int gnutls_get_data_buffer(ContentType type, GNUTLS_STATE state, char *data, int length)
{
	if (length < 0 || data==NULL) {
		gnutls_assert();
		return GNUTLS_E_INVALID_PARAMETERS;
	}
	
	switch(type) {
	case GNUTLS_APPLICATION_DATA:
	
		if (length > state->gnutls_internals.buffer.size) {
			length = state->gnutls_internals.buffer.size;
		}
#ifdef BUFFERS_DEBUG
	_gnutls_log( "RECORD BUFFER: Read %d bytes of Data(%d)\n", length, type);
#endif
		state->gnutls_internals.buffer.size -= length;
		memcpy(data, state->gnutls_internals.buffer.data, length);

		/* overwrite buffer */
		memcpy(state->gnutls_internals.buffer.data,
			&state->gnutls_internals.buffer.data[length],
			state->gnutls_internals.buffer.size);
		state->gnutls_internals.buffer.data =
		    gnutls_realloc_fast(state->gnutls_internals.buffer.data,
				   state->gnutls_internals.buffer.size);
		break;
		
	case GNUTLS_HANDSHAKE:
		if (length > state->gnutls_internals.buffer_handshake.size) {
			length = state->gnutls_internals.buffer_handshake.size;
		}
#ifdef BUFFERS_DEBUG
	_gnutls_log( "HANDSHAKE BUFFER: Read %d bytes of Data(%d)\n", length, type);
#endif
		state->gnutls_internals.buffer_handshake.size -= length;
		memcpy(data, state->gnutls_internals.buffer_handshake.data, length);

		/* overwrite buffer */
		memcpy(state->gnutls_internals.buffer_handshake.data,
			&state->gnutls_internals.buffer_handshake.data[length],
			state->gnutls_internals.buffer_handshake.size);
		state->gnutls_internals.buffer_handshake.data =
		    gnutls_realloc_fast(state->gnutls_internals.buffer_handshake.data,
				   state->gnutls_internals.buffer_handshake.size);
		break;
	default:
		gnutls_assert();
		return GNUTLS_E_INVALID_PARAMETERS;
	}


	return length;
}


/* This function is like read. But it does not return -1 on error.
 * It does return gnutls_errno instead.
 *
 * Flags are only used if the default recv() function is being used.
 */
static ssize_t _gnutls_read( SOCKET fd, GNUTLS_STATE state, void *iptr, size_t sizeOfPtr, int flags)
{
	size_t left;
	ssize_t i=0;
	char *ptr = iptr;
#ifdef READ_DEBUG
	int j,x, sum=0;
#endif


	left = sizeOfPtr;
	while (left > 0) {
		
		if (state->gnutls_internals._gnutls_pull_func==NULL)
			i = recv(fd, &ptr[sizeOfPtr-left], left, flags);
		else
			i = state->gnutls_internals._gnutls_pull_func(fd, &ptr[sizeOfPtr-left], left);
				
		if (i < 0) {
#ifdef READ_DEBUG
			_gnutls_log( "READ: %d returned from %d, errno=%d\n", i, fd, errno);
#endif
			if (errno == EAGAIN || errno == EINTR) {
				if (sizeOfPtr-left > 0) {
#ifdef READ_DEBUG
					_gnutls_log( "READ: returning %d bytes from %d\n", sizeOfPtr-left, fd);
#endif
					goto finish;
				}
				gnutls_assert();

				return RET(errno);
			} else {
				gnutls_assert();
				return GNUTLS_E_PULL_ERROR;
			}
		} else {
#ifdef READ_DEBUG
			_gnutls_log( "READ: Got %d bytes from %d\n", i, fd);
#endif
			if (i == 0)
				break;	/* EOF */
		}

		left -= i;

	}

	finish:
	
#ifdef READ_DEBUG
	_gnutls_log( "READ: read %d bytes from %d\n", (sizeOfPtr-left), fd);
	for (x=0;x<((sizeOfPtr-left)/16)+1;x++) {
		_gnutls_log( "%.4x - ",x);
		for (j=0;j<16;j++) {
			if (sum<(sizeOfPtr-left)) {
				_gnutls_log( "%.2x ", ((unsigned char*)ptr)[sum++]);
			}
		}
		_gnutls_log( "\n");
	
	}
#endif


	return (sizeOfPtr - left);
}


#define RCVLOWAT state->gnutls_internals.lowat 

int _gnutls_clear_peeked_data( SOCKET cd, GNUTLS_STATE state) {
char peekdata1[10];
char *peekdata2;
char * peek;
int ret, sum;

	if (state->gnutls_internals.have_peeked_data==0 || RCVLOWAT==0)
		return 0;
		
	if (RCVLOWAT > sizeof(peekdata1)) {
		peekdata2 = gnutls_malloc( RCVLOWAT);
		if (peekdata2==NULL) {
			gnutls_assert();
			return GNUTLS_E_MEMORY_ERROR;
		}
		
		peek = peekdata2;
		
        } else {
        	peek = peekdata1;
        }

        /* this was already read by using MSG_PEEK - so it shouldn't fail */
	sum = 0;
        do { /* we need this to finish now */
        	ret = _gnutls_read( cd, state, peek, RCVLOWAT-sum, 0);
        	if (ret > 0) sum+=ret;
       	} while( ret==GNUTLS_E_INTERRUPTED || ret==GNUTLS_E_AGAIN || sum < RCVLOWAT);

	if (peek==peekdata2) {
		gnutls_free(peekdata2);
	}
	
	if (ret < 0) {
		gnutls_assert();
		return ret;
	}

	state->gnutls_internals.have_peeked_data=0;

       	return 0;
}


void _gnutls_clear_read_buffer( GNUTLS_STATE state) {
	state->gnutls_internals.recv_buffer.size = 0;
}
                  	
/* This function is like recv(with MSG_PEEK). But it does not return -1 on error.
 * It does return gnutls_errno instead.
 * This function reads data from the socket and keeps them in a buffer, of up to
 * MAX_RECV_SIZE. 
 *
 * sizeOfPtr should be unsigned.
 *
 * This is not a general purpose function. It returns EXACTLY the data requested.
 *
 */
ssize_t _gnutls_read_buffered( int fd, GNUTLS_STATE state, opaque **iptr, size_t sizeOfPtr, ContentType recv_type)
{
	ssize_t ret=0, ret2=0;
	int min, buf_pos;
	char *buf;
	int recvlowat = RCVLOWAT;
	int recvdata;

	*iptr = state->gnutls_internals.recv_buffer.data;

	if ( sizeOfPtr > MAX_RECV_SIZE || sizeOfPtr == 0 
	|| (state->gnutls_internals.recv_buffer.size+sizeOfPtr) > MAX_RECV_SIZE) {
		gnutls_assert(); /* internal error */
		return GNUTLS_E_INVALID_PARAMETERS;
	}
	
	/* leave peeked data to the kernel space only if application data
	 * is received and we don't have any peeked 
	 * data in gnutls state.
	 */
	if ( recv_type != GNUTLS_APPLICATION_DATA
		&& state->gnutls_internals.have_peeked_data==0)
		recvlowat = 0;

	
	/* calculate the actual size, ie. get the minimum of the
	 * buffered data and the requested data.
	 */
	min = GMIN( state->gnutls_internals.recv_buffer.size, sizeOfPtr);
	if ( min > 0) {
		/* if we have enough buffered data
		 * then just return them.
		 */
		if ( min == sizeOfPtr) {
			return min;
		}
	}

	/* min is over zero. recvdata is the data we must
	 * receive in order to return the requested data.
	 */
	recvdata = sizeOfPtr - min;
	
	/* Allocate the data required to store the new packet.
	 */
	state->gnutls_internals.recv_buffer.data = gnutls_realloc_fast(
		state->gnutls_internals.recv_buffer.data, recvdata+state->gnutls_internals.recv_buffer.size);
	if ( state->gnutls_internals.recv_buffer.data==NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	buf_pos = state->gnutls_internals.recv_buffer.size;
	buf = state->gnutls_internals.recv_buffer.data;
	*iptr = buf;

	/* READ DATA - but leave RCVLOWAT bytes in the kernel buffer.
	 */
	if ( recvdata - recvlowat > 0) {
		ret = _gnutls_read( fd, state, &buf[buf_pos], recvdata - recvlowat, 0);

		/* return immediately if we got an interrupt or eagain
		 * error.
		 */
		if (ret < 0 && gnutls_is_fatal_error(ret)==0) {
			return ret;
		}
	}

#ifdef READ_DEBUG
	if (ret > 0)
		_gnutls_log("RB: Have %d bytes into buffer. Adding %d bytes.\nRB: Requested %d bytes\n", state->gnutls_internals.recv_buffer.size, ret, sizeOfPtr);
#endif
	/* copy fresh data to our buffer.
	 */
	if (ret > 0)
		state->gnutls_internals.recv_buffer.size += ret;


	buf_pos = state->gnutls_internals.recv_buffer.size;
	
	/* This is hack in order for select to work. Just leave recvlowat data,
	 * into the kernel buffer (using a read with MSG_PEEK), thus making
	 * select think, that the socket is ready for reading
	 */
	if (ret == (recvdata - recvlowat) && recvlowat > 0) {
		ret2 = _gnutls_read( fd, state, &buf[buf_pos], recvlowat, MSG_PEEK);

		if (ret2 < 0 && gnutls_is_fatal_error(ret2)==0) {
			return ret2;
		}

#ifdef READ_DEBUG
		if (ret2 > 0) {
			_gnutls_log("RB-PEEK: Read %d bytes in PEEK MODE.\n", ret2); 
			_gnutls_log("RB-PEEK: Have %d bytes into buffer. Adding %d bytes.\nRB: Requested %d bytes\n", state->gnutls_internals.recv_buffer.size, ret2, sizeOfPtr);
		}
#endif

		if (ret2 > 0) {
			state->gnutls_internals.have_peeked_data = 1;
			state->gnutls_internals.recv_buffer.size += ret2;

		}
	}

	if (ret < 0 || ret2 < 0) {
		gnutls_assert();
		/* that's because they are initilized to 0 */
		return GMIN(ret, ret2);
	}

	ret += ret2;

	if (ret > 0 && ret < recvlowat) {
		gnutls_assert();
		return GNUTLS_E_AGAIN;
	}
	
	if (ret==0) { /* EOF */
		gnutls_assert();
		return 0;
	}

	ret = state->gnutls_internals.recv_buffer.size;

	if ((ret > 0) && (ret < sizeOfPtr)) {
		/* Short Read */
		gnutls_assert();
		return GNUTLS_E_AGAIN;
	} else {
		return ret;
	}
}


/* These two functions are used to insert data to the send buffer of the handshake or
 * record protocol. The send buffer is kept if a send is interrupted and we need to keep
 * the data left to sent, in order to send them later.
 */
 
#define MEMSUB(x,y) (x-y)

inline
static int _gnutls_buffer_insert( gnutls_datum * buffer, const opaque* _data, int data_size) {

	if ( ( MEMSUB(_data, buffer->data) >= 0) && (MEMSUB(_data, buffer->data) < buffer->size) ) {
		/* the given _data is part of the buffer.
		 */
		if (data_size > buffer->size) {
			gnutls_assert();
			/* this shouldn't have happened */
			return GNUTLS_E_UNKNOWN_ERROR;
		}
		
		if (_data==buffer->data) { /* then don't even memmove */
			buffer->size = data_size;
			return 0;
		}
		
		memmove( buffer->data, _data, data_size);
		buffer->size = data_size;

		return 0;		
	}
	
	buffer->data = gnutls_realloc_fast( buffer->data, data_size);
	buffer->size = data_size;
	
	if (buffer->data == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	memcpy( buffer->data, _data, data_size);

	return 0;
}

inline
static int _gnutls_buffer_get( gnutls_datum * buffer, const opaque ** ptr, size_t *ptr_size) {
	*ptr_size = buffer->size;
	*ptr = buffer->data;
	
	return 0;
}

/* This function is like write. But it does not return -1 on error.
 * It does return gnutls_errno instead.
 *
 * In case of E_AGAIN and E_INTERRUPTED errors, you must call gnutls_write_flush(),
 * until it returns ok (0).
 *
 * We need to push exactly the data in n, since we cannot send less
 * data. In TLS the peer must receive the whole packet in order
 * to decrypt and verify the integrity. 
 *
 */
ssize_t _gnutls_write_buffered(SOCKET fd, GNUTLS_STATE state, const void *iptr, size_t n)
{
	size_t left;
#ifdef WRITE_DEBUG
	int j,x, sum=0;
#endif
	ssize_t retval, i;
	const opaque * ptr;
	int ret;
	
	ptr = iptr;
	
	/* In case the previous write was interrupted, check if the
	 * iptr != NULL and we have data in the buffer.
	 * If this is true then return an error.
	 */
	if (state->gnutls_internals.send_buffer.size > 0 && iptr != NULL) {
		gnutls_assert();
		return GNUTLS_E_INVALID_PARAMETERS;
	}

	/* If data in the buffer exist
	 */
	if (iptr == NULL) {
		/* checking is handled above */
		ret = _gnutls_buffer_get( &state->gnutls_internals.send_buffer, &ptr, &n);
		if (ret < 0) {
			gnutls_assert();
			return retval;
		}
#ifdef WRITE_DEBUG
		_gnutls_log( "WRITE: Restoring old write. (%d bytes to send)\n", n);
#endif
	}

#ifdef WRITE_DEBUG
	_gnutls_log( "WRITE: Will write %d bytes to %d.\n", n, fd);
#endif

	i = 0;
	left = n;
	while (left > 0) {
		
		if (state->gnutls_internals._gnutls_push_func==NULL) 
			i = send(fd, &ptr[n-left], left, 0);
		else
			i = state->gnutls_internals._gnutls_push_func(fd, &ptr[n-left], left);

		if (i == -1) {
			if (errno == EAGAIN || errno == EINTR) {
				state->gnutls_internals.send_buffer_prev_size += n - left;

				retval = _gnutls_buffer_insert( &state->gnutls_internals.send_buffer, &ptr[n-left], left);
				if (retval < 0) {
					gnutls_assert();
					return retval;
				}
				
#ifdef WRITE_DEBUG
				_gnutls_log( "WRITE: Interrupted. Stored %d bytes to buffer. Already sent %d bytes.\n", left, n-left);
#endif
				retval = RET(errno);

				return retval;
			} else {
				gnutls_assert();
				return GNUTLS_E_PUSH_ERROR;
			}
		}
		left -= i;

#ifdef WRITE_DEBUG
		_gnutls_log( "WRITE: wrote %d bytes to %d. Left %d bytes. Total %d bytes.\n", i, fd, left, n);
		for (x=0;x<((i)/16)+1;x++) {
			if (sum>n-left)
				break;

			_gnutls_log( "%.4x - ",x);
			for (j=0;j<16;j++) {
				if (sum<n-left) {
					_gnutls_log( "%.2x ", ((unsigned char*)ptr)[sum++]);
				} else break;
			}
			_gnutls_log( "\n");
		}
		_gnutls_log( "\n");
#endif

	}

	retval = n + state->gnutls_internals.send_buffer_prev_size;

	state->gnutls_internals.send_buffer.size = 0;
	state->gnutls_internals.send_buffer_prev_size = 0;

	return retval;

}

/* This function writes the data that are left in the
 * TLS write buffer (ie. because the previous write was
 * interrupted.
 */
ssize_t _gnutls_write_flush(SOCKET fd, GNUTLS_STATE state)
{
    ssize_t ret;

    if (state->gnutls_internals.send_buffer.size == 0)
        return 0; /* done */

    ret = _gnutls_write_buffered(fd, state, NULL, 0);
#ifdef WRITE_DEBUG 
    _gnutls_log("WRITE FLUSH: %d [buffer: %d]\n", ret, state->gnutls_internals.send_buffer.size);
#endif

    return ret;
}

/* This function writes the data that are left in the
 * Handshake write buffer (ie. because the previous write was
 * interrupted.
 */
ssize_t _gnutls_handshake_write_flush(SOCKET fd, GNUTLS_STATE state)
{
    ssize_t ret;
    ret = _gnutls_handshake_send_int(fd, state, 0, 0, NULL, 0);
    if (ret < 0) {
	gnutls_assert();
    	return ret;
    }
#ifdef WRITE_DEBUG
    _gnutls_log("HANDSHAKE_FLUSH: written[1] %d bytes\n", ret);
#endif

    if (state->gnutls_internals.handshake_send_buffer.size == 0) {
	ret = state->gnutls_internals.handshake_send_buffer_prev_size; /* done */
	state->gnutls_internals.handshake_send_buffer_prev_size = 0;
	state->gnutls_internals.handshake_send_buffer.size = 0;

	return ret;
    }

    return ret;
}


/* This is a send function for the gnutls handshake 
 * protocol. Just makes sure that all data have been sent.
 */
ssize_t _gnutls_handshake_send_int( SOCKET fd, GNUTLS_STATE state, ContentType type, HandshakeType htype, const void *iptr, size_t n)
{
	size_t left;
	ssize_t i = 0, ret=0;
	const opaque *ptr;
        ssize_t retval = 0;

	ptr = iptr;
       
	if (state->gnutls_internals.handshake_send_buffer.size > 0 && ptr==NULL && n == 0) {
		/* resuming previously interrupted write
		 */
		gnutls_assert(); 
		ret = _gnutls_buffer_get( &state->gnutls_internals.handshake_send_buffer, &ptr, &n);
		if (ret < 0) {
			gnutls_assert();
			return retval;
		}

		type = state->gnutls_internals.handshake_send_buffer_type;
		htype = state->gnutls_internals.handshake_send_buffer_htype;

	} else if (state->gnutls_internals.handshake_send_buffer.size > 0) {
		gnutls_assert();
		return GNUTLS_E_UNKNOWN_ERROR;
	} else {
#ifdef WRITE_DEBUG
		size_t sum=0, x, j;
		
		_gnutls_log( "HWRITE: will write %d bytes to %d.\n", n, fd);
		for (x=0;x<((n)/16)+1;x++) {
			if (sum>n)
				break;

			_gnutls_log( "%.4x - ",x);
			for (j=0;j<16;j++) {
				if (sum<n) {
					_gnutls_log( "%.2x ", ((unsigned char*)ptr)[sum++]);
				} else break;
			}
			_gnutls_log( "\n");
		}
		_gnutls_log( "\n");
#endif

	
	}

	if (n==0) { /* if we have no data to send */
		gnutls_assert();
		return 0;
	} else if (ptr==NULL) {
		gnutls_assert();
		return GNUTLS_E_UNKNOWN_ERROR;
	}
	

	left = n;
	while (left > 0) {
		ret = gnutls_send_int(fd, state, type, htype, &ptr[n-left], left);

		if (ret <= 0) {
			if (ret==0) {
				gnutls_assert();
				ret = GNUTLS_E_UNKNOWN_ERROR;
			}

			if ( left > 0 && (ret==GNUTLS_E_INTERRUPTED || ret==GNUTLS_E_AGAIN)) { 
				gnutls_assert();

				retval = _gnutls_buffer_insert( &state->gnutls_internals.handshake_send_buffer, &ptr[n-left], left);
				if (retval < 0) {
					gnutls_assert();
					return retval;
				}

				state->gnutls_internals.handshake_send_buffer_prev_size += n-left;

				state->gnutls_internals.handshake_send_buffer_type = type;
				state->gnutls_internals.handshake_send_buffer_htype = htype;

			} else {
				state->gnutls_internals.handshake_send_buffer_prev_size = 0;
				state->gnutls_internals.handshake_send_buffer.size = 0;
			}

			gnutls_assert();
			return ret;
		}
		i = ret;
		left -= i;
	}

	retval = n + state->gnutls_internals.handshake_send_buffer_prev_size;

	state->gnutls_internals.handshake_send_buffer.size = 0;
	state->gnutls_internals.handshake_send_buffer_prev_size = 0;

	return retval;

}

/* This is a receive function for the gnutls handshake 
 * protocol. Makes sure that we have received all data.
 */
ssize_t _gnutls_handshake_recv_int(int fd, GNUTLS_STATE state, ContentType type, HandshakeType htype, void *iptr, size_t sizeOfPtr)
{
	size_t left;
	ssize_t i;
	char *ptr;
	size_t dsize;
		
	ptr = iptr;
	left = sizeOfPtr;

	if (sizeOfPtr == 0 || iptr == NULL) {
		gnutls_assert();
		return GNUTLS_E_INVALID_PARAMETERS;
	}

	if (state->gnutls_internals.handshake_recv_buffer.size > 0) {
		/* if we have already received some data */
		if (sizeOfPtr <= state->gnutls_internals.handshake_recv_buffer.size) {
			/* if requested less data then return it.
			 */
			gnutls_assert();
			memcpy( iptr, state->gnutls_internals.handshake_recv_buffer.data, sizeOfPtr);

			state->gnutls_internals.handshake_recv_buffer.size -= sizeOfPtr;

			memmove( state->gnutls_internals.handshake_recv_buffer.data, 
				&state->gnutls_internals.handshake_recv_buffer.data[sizeOfPtr], 
				state->gnutls_internals.handshake_recv_buffer.size);
			
			return sizeOfPtr;
		}
		gnutls_assert();
		memcpy( iptr, state->gnutls_internals.handshake_recv_buffer.data, state->gnutls_internals.handshake_recv_buffer.size);

		htype = state->gnutls_internals.handshake_recv_buffer_htype;
		type = state->gnutls_internals.handshake_recv_buffer_type;

		left -= state->gnutls_internals.handshake_recv_buffer.size;

		state->gnutls_internals.handshake_recv_buffer.size = 0;
	}
	
	while (left > 0) {
		dsize = sizeOfPtr - left;
		i = gnutls_recv_int(fd, state, type, htype, &ptr[dsize], left);
		if (i < 0) {
			
			if (dsize > 0 && (i==GNUTLS_E_INTERRUPTED || i==GNUTLS_E_AGAIN)) {
				gnutls_assert();

				state->gnutls_internals.handshake_recv_buffer.data = gnutls_realloc_fast(
					state->gnutls_internals.handshake_recv_buffer.data, dsize);
				if (state->gnutls_internals.handshake_recv_buffer.data==NULL) {
					gnutls_assert();
					return GNUTLS_E_MEMORY_ERROR;
				}
							
				memcpy( state->gnutls_internals.handshake_recv_buffer.data, iptr, dsize);

				state->gnutls_internals.handshake_recv_buffer_htype = htype;
				state->gnutls_internals.handshake_recv_buffer_type = type;

				state->gnutls_internals.handshake_recv_buffer.size = dsize;
			} else 
				state->gnutls_internals.handshake_recv_buffer.size = 0;

			gnutls_assert();

			return i;
		} else {
			if (i == 0)
				break;	/* EOF */
		}

		left -= i;

	}

	state->gnutls_internals.handshake_recv_buffer.size = 0;

	return sizeOfPtr - left;
}

/* Buffer for handshake packets. Keeps the packets in order
 * for finished messages to use them. Used in HMAC calculation
 * and finished messages.
 */
int gnutls_insert_to_handshake_buffer( GNUTLS_STATE state, char *data, int length)
{
	int old_buffer;

	if (length==0) return 0;	
	old_buffer = state->gnutls_internals.hash_buffer.size;

	state->gnutls_internals.hash_buffer.size += length;
	if (state->gnutls_internals.max_handshake_data_buffer_size > 0 && state->gnutls_internals.hash_buffer.size > state->gnutls_internals.max_handshake_data_buffer_size) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

#ifdef BUFFERS_DEBUG
	_gnutls_log( "HASH BUFFER: Inserted %d bytes of Data\n", length);
#endif
	state->gnutls_internals.hash_buffer.data =
		    gnutls_realloc_fast(state->gnutls_internals.hash_buffer.data,
			   state->gnutls_internals.hash_buffer.size);
	if (state->gnutls_internals.hash_buffer.data == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}
	memcpy(&state->gnutls_internals.hash_buffer.data[old_buffer], data, length);

	return 0;
}

int gnutls_get_handshake_buffer_size( GNUTLS_STATE state)
{

	return state->gnutls_internals.hash_buffer.size;
}

int gnutls_get_handshake_buffer( GNUTLS_STATE state, char *data, int length)
{
	if (length > state->gnutls_internals.hash_buffer.size) {
		length = state->gnutls_internals.hash_buffer.size;
	}
#ifdef BUFFERS_DEBUG
	_gnutls_log( "HASH BUFFER: Got %d bytes of Data\n", length);
#endif
	state->gnutls_internals.hash_buffer.size -= length;
	memcpy(data, state->gnutls_internals.hash_buffer.data, length);
	/* overwrite buffer */
	memcpy(state->gnutls_internals.hash_buffer.data,
		&state->gnutls_internals.hash_buffer.data[length],
		state->gnutls_internals.hash_buffer.size);
	state->gnutls_internals.hash_buffer.data =
	    gnutls_realloc_fast(state->gnutls_internals.hash_buffer.data,
			   state->gnutls_internals.hash_buffer.size);

	if (state->gnutls_internals.hash_buffer.data == NULL) {
		gnutls_assert();
		return GNUTLS_E_MEMORY_ERROR;
	}

	return length;	

}

/* this function does not touch the buffer
 * and returns data from it (peek mode!)
 */
int gnutls_read_handshake_buffer( GNUTLS_STATE state, char *data, int length)
{
	if (length > state->gnutls_internals.hash_buffer.size) {
		length = state->gnutls_internals.hash_buffer.size;
	}
#ifdef BUFFERS_DEBUG
	_gnutls_log( "HASH BUFFER: Read %d bytes of Data\n", length);
#endif
	memcpy(data, state->gnutls_internals.hash_buffer.data, length);
	return length;	
}



int gnutls_clear_handshake_buffer( GNUTLS_STATE state)
{

#ifdef BUFFERS_DEBUG
	_gnutls_log( "HASH BUFFER: Cleared Data from buffer\n");
#endif
	state->gnutls_internals.hash_buffer.size = 0;
	if (state->gnutls_internals.hash_buffer.data!=NULL)
		gnutls_free(state->gnutls_internals.hash_buffer.data);
	state->gnutls_internals.hash_buffer.data = NULL;
	
	return 0;
}
