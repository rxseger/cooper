import machine
import time
import micropython

micropython.alloc_emergency_exception_buf(100)

#from umqtt.simple import MQTTClient

CONFIG = {
    "broker": "192.168.1.19", # overwritten by contents of file "/broker.ip"
    "client_id": b"esp8266_bedroom",
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
    try:
        with open("/broker.ip") as f:
            # All configuration is specified above except this one field can be overridden
            CONFIG['broker'] = f.read().strip()
    except (OSError, ValueError):
        print("Couldn't load /broker.ip, assuming default broker {}".format(CONFIG['broker']))
    print("Loaded config from /broker.ip")

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
