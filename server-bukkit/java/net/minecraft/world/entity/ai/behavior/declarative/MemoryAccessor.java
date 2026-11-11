// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.declarative;
import com.mojang.datafixers.kinds.App;
import com.mojang.datafixers.kinds.K1;
import java.util.Optional;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
public final class MemoryAccessor<F extends K1, Value> {
    private final Brain<?> brain;
    private final MemoryModuleType<Value> memoryType;
    private final App<F, Value> value;
    public MemoryAccessor(Brain<?> brain, MemoryModuleType<Value> memoryType, App<F, Value> value) {
        this.brain = brain;
        this.memoryType = memoryType;
        this.value = value;
    }
    public App<F, Value> value() {
        return value;
    }
    public void set(Value value) {
        brain.setMemory(memoryType, Optional.of(value));
    }
    public void setOrErase(Optional<Value> value) {
        brain.setMemory(memoryType, value);
    }
    public void setWithExpiry(Value value, long timeToLive) {
        brain.setMemoryWithExpiry(memoryType, value, timeToLive);
    }
    public void erase() {
        brain.eraseMemory(memoryType);
    }
}
