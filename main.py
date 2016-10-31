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

global gpio_changed, any_gpio_changed
gpio_changed = bytearray(16) # sixteen bytes (position=pin #), 0=not changed, 1=changed
any_gpio_changed = False

def main():
    global gpio_changed, any_gpio_changed
    load_config()
    analog_pin = machine.ADC(0) # pin A0 on ESP8266
    client = MQTTClient(CONFIG['client_id'], CONFIG['broker'])
    client.connect()
    print("Connected to {}".format(CONFIG['broker']))

    # interrupt-driven switches
    print('Switches:',CONFIG['switches'])
    name2pin = {}
    pin_number2name = {}
    for info in CONFIG['switches']:
        pin_number = info['pin']
        name = info['name']

        pin = machine.Pin(pin_number, machine.Pin.IN, info['pull_up_down'])
        name2pin[name] = pin
        pin_number2name[pin_number] = name

        def add_handler(pin_number, name, trigger):
            def handler(ignored): # ISR argument is a Pin object, which can't convert back to a pin number?! Ignore it and use closure
                print('pin change {}',pin_number)
                global gpio_changed, any_gpio_changed
                gpio_changed[pin_number] = 1 # flag this pin as having changed
                any_gpio_changed = True
                # important: can't do much in an ISR, see https://micropython.org/resources/docs/en/latest/wipy/reference/isr_rules.html
                #print('read {}: {}'.format(pin,pin.value()))
            print('Watching pin {} = {}'.format(pin_number, name))
            pin.irq(trigger=trigger, handler=handler)

        add_handler(pin_number, name, info['trigger'])

    # poll
    while True:
        data = analog_pin.read()
        client.publish('{}/{}'.format(CONFIG['topic'],
                                          CONFIG['client_id']),
                                          bytes(str(data), 'utf-8'))
        print('Sensor state: {}'.format(data))

        if any_gpio_changed:
            print('Saw some GPIO change:',gpio_changed)
            # Critical section: accumulate list of changed GPIO pins
            list_gpio_changed = []
            #pyb.disable_irq() # note: not pyb as in https://micropython.org/resources/docs/en/latest/wipy/reference/isr_rules.html - see https://github.com/micropython/micropython/issues/2085
            #machine.disable_irq() # TODO: why does this seem to hang the program over the console, even though I re-enable interrupts?
            for pin, flag in enumerate(gpio_changed):
                if flag:
                    list_gpio_changed.append(pin)
            any_gpio_changed = False
            gpio_changed = bytearray(16)
            #machine.enable_irq()

            # Get what it changed to
            for pin_number in list_gpio_changed:
                name = pin_number2name[pin_number]
                pin = name2pin[name]

                value = pin.value()
            
                print('GPIO pin changed: {} -> {}'.format(name, value))


        time.sleep(1) # TODO: configurable interval

# TODO: re-enable
#if __name__ == '__main__':
#    main()
