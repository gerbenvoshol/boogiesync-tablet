// Compile with: gcc -o blue blue.c -lbluetooth -Wall -ggdb -levdev -I /usr/include/libevdev-1.0/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>


int main(int argc, char **argv)
{
    inquiry_info *inquiry_infos = NULL;
    int max_responses, num_responses;
    int device_id, sock, len, flags;
    int i;
    char addr[248] = { 0 };
    char device_name[248] = { 0 };
    int found = 0;
    
    if (argc == 1 ) {
        printf("No device address given, will try and find it\n");
    } else if (argc == 2) {
        found = 1;
        strcpy(addr, argv[1]);
    }

    device_id = hci_get_route(NULL);
    sock = hci_open_dev( device_id );
    if (device_id < 0 || socket < 0) {
        perror("opening socket");
        exit(1);
    }

    len  = 8;
    max_responses = 255;
    flags = IREQ_CACHE_FLUSH;
    inquiry_infos = (inquiry_info*)malloc(max_responses * sizeof(inquiry_info));

    while (!found) {
        num_responses = hci_inquiry(device_id, len, max_responses, NULL, &inquiry_infos, flags);
        if( num_responses < 0 ) perror("hci_inquiry");

        for (i = 0; i < num_responses; i++) {
            ba2str(&(inquiry_infos+i)->bdaddr, addr);
            memset(device_name, 0, sizeof(device_name));
            if (hci_read_remote_name(sock, &(inquiry_infos+i)->bdaddr, sizeof(device_name), 
                device_name, 0) < 0)
            strcpy(device_name, "[unknown]");
            // e.g. A0:E6:F8:A1:72:DD  Sync
            if (!strcmp("Sync", device_name)) {
                printf("Found: %s  %s\n", addr, device_name);
                found = 1;
                break;
            }
        }
    }

    close( sock );

    //uuid = "d6a56f80-88f8-11e3-baa8-0800200c9a66";
    uint8_t svc_uuid_int[] = { 0xd6, 0xa5, 0x6f, 0x80, 0x88, 0xf8, 0x11, 0xe3, 0xba, 0xa8, 0x08, 0x00, 
        0x20, 0x0c, 0x9a, 0x66 };
    uuid_t svc_uuid;
    int err;
    bdaddr_t target;
    sdp_list_t *response_list = NULL, *search_list, *attrid_list;
    sdp_session_t *session = 0;

    str2ba( addr, &target );

    // connect to the SDP server running on the remote machine
    session = sdp_connect( BDADDR_ANY, &target, SDP_RETRY_IF_BUSY );

    // specify the UUID of the application we're searching for
     sdp_uuid128_create( &svc_uuid, &svc_uuid_int );
   // bt_string_to_uuid128(&svc_uuid, uuid);
    search_list = sdp_list_append( NULL, &svc_uuid );

    // specify that we want a list of all the matching applications' attributes
    uint32_t range = 0x0000ffff;
    attrid_list = sdp_list_append( NULL, &range );

    // get a list of service records that have UUID 0xabcd
    err = sdp_service_search_attr_req( session, search_list, \
            SDP_ATTR_REQ_RANGE, attrid_list, &response_list);

    sdp_list_t *r = response_list;

    uint8_t channel = 0;

    // go through each of the service records
    for (; r; r = r->next ) {
        sdp_record_t *rec = (sdp_record_t*) r->data;
        sdp_list_t *proto_list;
        
        // get a list of the protocol sequences
        if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
        sdp_list_t *p = proto_list;
        
        printf("Found service record 0x%x\n", rec->handle);

        // go through each protocol sequence
        for( ; p ; p = p->next ) {
            sdp_list_t *pds = (sdp_list_t*)p->data;

            // go through each protocol list of the protocol sequence
            for( ; pds ; pds = pds->next ) {

                // check the protocol attributes
                sdp_data_t *d = (sdp_data_t*)pds->data;
                int proto = 0;
                for( ; d; d = d->next ) {
                    switch( d->dtd ) { 
                        case SDP_UUID16:
                        case SDP_UUID32:
                        case SDP_UUID128:
                            proto = sdp_uuid_to_proto( &d->val.uuid );
                            break;
                        case SDP_UINT8:
                            if( proto == RFCOMM_UUID ) {
                                printf("Found rfcomm channel: %d\n",d->val.int8);
                                channel = d->val.int8;
                            }
                            break;
                    }
                }
            }
            sdp_list_free( (sdp_list_t*)p->data, 0 );
        }
        
        sdp_record_free( rec );

        sdp_list_free( proto_list, 0 );

        }
    }

    sdp_close(session);

    printf("Trying to connect to: %s using channel %i\n", addr, channel);

    struct sockaddr_rc sockaddr = { 0 };
    int status;

    // allocate a socket
    sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // set the connection parameters (who to connect to)
    sockaddr.rc_family = AF_BLUETOOTH;
    sockaddr.rc_channel = channel;
    str2ba( addr, &sockaddr.rc_bdaddr );

    // connect to server
    status = connect(sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr));

    uint8_t resp[] = { 0xc0, 0x00, 0x00, 0xb8, 0xf0, 0xc0 };
    uint8_t payload[] = { 0xc0, 0x00, 0x53, 0x05, 0x05, 0x00, 0x04, 0x10, 0x71, 0xc0 };
    uint8_t end[] = { 0xc0 };

    int bytes_read;
    uint8_t buf[1024] = { 0 };

    // send a message
    if( status == 0 ) {
        printf("Now connected and trying to verify device\n");

        status = write(sock, payload, sizeof(payload));
        status += write(sock, end, sizeof(end));
        
        printf("Wrote %i bytes of data\n", status);

        bytes_read = read(sock, buf, sizeof(buf));
        if( bytes_read > 0 ) {
            printf("Received %i out of %li bytes [%x%x%x%x%x%x]\n", bytes_read, sizeof(resp), buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
            if (!memcmp(buf, resp, sizeof(resp))) {
                printf("Correct response\n");
            }
        }
    }

    if( status < 0 ) perror("uh oh");

    printf("Setting up input device\n");

    // # Configure UInput device
    // # Maximum position possible
    int minxpos = 60;
    int minypos = 135;
    int maxxpos = 19993;
    int maxypos = 13847;
    int minpressure = 0; //# 19
    int maxpressure = 255;

    struct libevdev *dev;
    struct libevdev_uinput *uidev;

    dev = libevdev_new();
    libevdev_set_name(dev, "boogie-board-sync");

    struct input_absinfo abs;

    // Initialise UInput device
    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_TOUCH, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_PEN, NULL);
    libevdev_enable_event_code(dev, EV_KEY, BTN_STYLUS, NULL);
    libevdev_enable_event_type(dev, EV_ABS);
    
    abs.minimum = minxpos;
    abs.maximum = maxxpos;
    abs.fuzz = 0;
    abs.flat = 0;
    abs.resolution = 44;
    libevdev_enable_event_code(dev, EV_ABS, ABS_X, &abs);

    abs.minimum = minypos;
    abs.maximum = maxypos;
    abs.fuzz = 0;
    abs.flat = 0;
    abs.resolution = 44;
    libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &abs);

    abs.minimum = minpressure;
    abs.maximum = maxpressure;
    abs.fuzz = 0;
    abs.flat = 0;
    abs.resolution = 44;    
    libevdev_enable_event_code(dev, EV_ABS, ABS_PRESSURE, &abs);

    err = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
    if (err < 0) {
        printf("Unable to setup device\n");
        return 1;
    }

    int true = 1;
    printf("Reading events\n");
    while (true) {
        bytes_read = read(sock, buf, sizeof(buf));

        if (bytes_read != 14) {// ||  buf[0] != 192 || buf[1] != 1 || buf[2] != 161) {
            continue;
        }

        int xpos = buf[4] | buf[5] << 8;
        int ypos = buf[6] | buf[7] << 8;
        int touch = buf[10] & 0x01;
        int stylus = (buf[10] & 0x02) >> 1;
        int pressure = touch * (buf[8] | buf[9] << 8);

        printf("\rxpos %i ypos %i touch %i stylus %i pressure %i      ", xpos, ypos, touch, stylus, pressure);

        if (xpos < minxpos) {
            minxpos = xpos;
            printf("updated minxpos to %i\n", minxpos);
        } else if (xpos > maxxpos) {
            maxxpos = xpos;
            printf("updated maxxpos to %i\n", maxxpos);
        } else if (ypos < minypos) {
            minypos = ypos;
            printf("updated minypos to %i\n", minypos);
        } else if (ypos > maxypos) {
            maxypos = ypos;
            printf("updated maxypos to %i\n", maxypos);
        } else if (pressure < minpressure) {
            minpressure = pressure;
            printf("updated minpressure to %i\n", minpressure);
        } else if (pressure > maxpressure) {
            maxpressure = pressure;
            printf("updated maxpressure to %i\n", maxpressure);
        }

        libevdev_uinput_write_event(uidev, EV_ABS, ABS_PRESSURE, pressure);
        libevdev_uinput_write_event(uidev, EV_ABS, ABS_X, xpos);
        libevdev_uinput_write_event(uidev, EV_ABS, ABS_Y, ypos);
        libevdev_uinput_write_event(uidev, EV_KEY, BTN_RIGHT, stylus);
        libevdev_uinput_write_event(uidev, EV_KEY, BTN_LEFT, touch);
        libevdev_uinput_write_event(uidev, EV_KEY, BTN_TOUCH, touch);
        libevdev_uinput_write_event(uidev, EV_KEY, BTN_STYLUS, stylus);
        libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
    }

    close(sock);

    // libevdev_uinput_write_event(uidev, EV_KEY, KEY_A, 1);
    // libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
    // libevdev_uinput_write_event(uidev, EV_KEY, KEY_A, 0);
    // libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);

    libevdev_uinput_destroy(uidev);
    printf("Complete\n");

    free( inquiry_infos );

    return 0;
}
