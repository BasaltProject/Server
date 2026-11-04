// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
public interface ScoreAccess {
    int get();
    void set(int value);
    default int add(int count) {
        int updated = get() + count;
        set(updated);
        return updated;
    }
    default int increment() {
        return add(1);
    }
    default void reset() {
        set(0);
    }
    boolean locked();
    void unlock();
    void lock();
    Component display();
    void display(Component display);
    void numberFormatOverride(NumberFormat numberFormat);
}
