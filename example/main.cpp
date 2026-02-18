
#include <EventBus.hpp>


struct AttackEvent {
    int damage = 0;
};

class Player {
public:
    void attack(AttackEvent &attack_event) {
        attack_event.damage += 150;
    }
};

void base_attack(AttackEvent& attack_event) {
    if (attack_event.damage <= 0) attack_event.damage = 1;
}

void attack(AttackEvent &base_event, EventBus<std::null_mutex> &event_bus) {
    event_bus.Publish(base_event);
}

Player player;

int main() {
    AttackEvent attack_event;

    EventBus<std::null_mutex> event_bus;
    event_bus.Subscribe(make_func(base_attack), EventPriority::High);
    event_bus.Subscribe(make_func(&Player::attack, &player));

    attack(attack_event, event_bus);

    printf("result damage: %d\n", attack_event.damage);
}