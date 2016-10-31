import machine
import time
import ubinascii
import webrepl
import micropython

micropython.alloc_emergency_exception_buf(100)

#from umqtt.simple import MQTTClient

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
    "interval": 0.01, # 10 ms
    "adc_count_interval": 500, # every N*interval, poll the ADC
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

global any_gpio_changed
any_gpio_changed = False

def main():
    global any_gpio_changed
    load_config()
    analog_pin = machine.ADC(0) # pin A0 on ESP8266
    #client = MQTTClient(CONFIG['client_id'], CONFIG['broker']) # TODO: what could cause "ImportError: cannot import name MQTTClient"?
    #client.connect()
    #print("Connected to {}".format(CONFIG['broker']))

    # interrupt-driven switches
    print('Switches:',CONFIG['switches'])
    name2pin = {}
    name2old_value = {}
    for info in CONFIG['switches']:
        pin_number = info['pin']
        name = info['name']

        pin = machine.Pin(pin_number, machine.Pin.IN, info['pull_up_down'])
        name2pin[name] = pin
        name2old_value[name] = 1 # active-low; assume starts off inactive

        def add_handler(pin_number, name, trigger):
            def handler(ignored): # ISR argument is a Pin object, which can't convert back to a pin number?! Ignore it and use closure
                print('pin change {}',pin_number)
                global any_gpio_changed
                any_gpio_changed = True
                # important: can't do much in an ISR, see https://micropython.org/resources/docs/en/latest/wipy/reference/isr_rules.html
                #print('read {}: {}'.format(pin,pin.value()))
            print('Watching pin {} = {}'.format(pin_number, name))
            pin.irq(trigger=trigger, handler=handler)

        add_handler(pin_number, name, info['trigger'])

    # poll
    i = 0
    while True:
        i += 1
        if i == CONFIG['adc_count_interval']:
            data = analog_pin.read()
            #TODO
            #client.publish('{}/{}'.format(CONFIG['topic'],
            #                                  CONFIG['client_id']),
            #                                  bytes(str(data), 'utf-8'))
            print('Sensor state: {}'.format(data))
            i = 0

        if any_gpio_changed:
            print('Saw some GPIO changes')
            any_gpio_changed = False

            # Get new values of all pins
            for name, pin in name2pin.items():
                old_value = name2old_value[name]
                value = pin.value()
                if old_value != value:
                    print('GPIO pin {}: {} -> {}'.format(name, old_value, value))

                # Save old value for next time
                name2old_value[name] = value

        time.sleep(CONFIG['interval']) # TODO: configurable interval

# TODO: re-enable
#if __name__ == '__main__':
#    main()
