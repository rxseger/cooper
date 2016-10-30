import machine
import time
import ubinascii
import webrepl
import micropython
micropython.alloc_emergency_exception_buf(100)

from umqtt.simple import MQTTClient

# These defaults are overwritten with the contents of /config.json by load_config()
CONFIG = {
    "broker": "192.168.1.19",
    "client_id": b"esp8266_" + ubinascii.hexlify(machine.unique_id()),
    "topic": b"home",
    "switches": [
        {
            "pin": 4,
            "name": "Switch #2",
            "pull_up_down": machine.Pin.PULL_UP,
            "trigger": machine.Pin.IRQ_RISING | machine.Pin.IRQ_FALLING,
        },
        {
            "pin": 2,
            "name": "Switch #3",
            "pull_up_down": machine.Pin.PULL_UP,
            "trigger": machine.Pin.IRQ_RISING | machine.Pin.IRQ_FALLING,
        },
        {
            "pin": 5,
            "name": "Switch #4",
            "pull_up_down": machine.Pin.PULL_UP,
            "trigger": machine.Pin.IRQ_RISING | machine.Pin.IRQ_FALLING,
        },
    ],
}

client = None

def load_config():
    import ujson as json
    try:
        with open("/config.json") as f:
            config = json.loads(f.read())
    except (OSError, ValueError):
        print("Couldn't load /config.json")
        save_config()
    else:
        CONFIG.update(config)
        print("Loaded config from /config.json")

def save_config():
    import ujson as json
    try:
        with open("/config.json", "w") as f:
            f.write(json.dumps(CONFIG))
    except OSError:
        print("Couldn't save /config.json")

global gpio_changed
gpio_changed = None

def main():
    global gpio_changed
    load_config()
    analog_pin = machine.ADC(0) # pin A0 on ESP8266
    client = MQTTClient(CONFIG['client_id'], CONFIG['broker'])
    client.connect()
    print("Connected to {}".format(CONFIG['broker']))

    # interrupt-driven switches
    print('Switches:',CONFIG['switches'])
    pins = {} # name to object
    for info in CONFIG['switches']:
        pin = machine.Pin(info['pin'], machine.Pin.IN, info['pull_up_down'])
        pins[info['name']] = pin
        def handler(what):
            global gpio_changed
            gpio_changed = what # this is about all we can do in an ISR # TODO: each pin independently
            #print('read {}: {}'.format(pin,pin.value()))
        print('Watching pin {} = {}'.format(info['pin'], info['name']))
        pin.irq(trigger=info['trigger'], handler=handler)

    # poll
    while True:
        data = analog_pin.read()
        client.publish('{}/{}'.format(CONFIG['topic'],
                                          CONFIG['client_id']),
                                          bytes(str(data), 'utf-8'))
        print('Sensor state: {}'.format(data))
        if gpio_changed is not None:
            print('GPIO pin changed: {}'.format(gpio_changed))
            for name, pin in pins.items():
                print('Pin {}: {}'.format(pin, pin.value()))
            gpio_changed = None

        time.sleep(1) # TODO: configurable interval

# TODO: re-enable
#if __name__ == '__main__':
#    main()
