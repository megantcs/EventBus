# EventBus 
a simple and convenient c++ event bus designed for easy and comfortable use in your projects.

Opportunities:
- convenient subscription for classes
- simple unsubscription for subscribers
- thread safety

## Demo

usage example:
```c++
#include <EventBus.hpp>

struct EventArgs {
    int param1 = 0;
};

void my_callback(EventArgs& arg) {
    arg.param1 = 100;
}
 
...
    EventBus event_bus;
    EventArgs event_args;
    
    event_bus.Subscribe(make_func(my_callback), EventPriority::Default);
    event_bus.Publish(event_bus);
    
    event_bus.Unsubscribe(make_func(my_callback));
    
    printf("result param1: %d\n", event_args.param1);
```
convenient subscription for classes:
```c++
#include <EventBus.hpp>

struct EventArgs {
    int param1 = 0;
};

class Program {
public:
    void Callback(EventArgs &event_args) {
        event_args.param1 = 10;
    }
};
...
    EventBus event_bus;
    EventArgs event_args;
    Program program;

    event_bus.Subscribe(make_func(&Program::Callback, &program), EventPriority::Default);
    event_bus.Publish(event_bus);

    event_bus.Unsubscribe(make_func(&Program::Callback, &program));

    printf("result param1: %d\n", event_args.param1);
```
## Disclaimer of liability
The project is a training project and is not ready for serious production. 
The project requires serious refactoring, use it at your own risk
