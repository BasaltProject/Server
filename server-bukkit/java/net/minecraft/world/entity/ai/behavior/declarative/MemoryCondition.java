// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior.declarative;
import com.mojang.datafixers.kinds.Const;
import com.mojang.datafixers.kinds.IdF;
import com.mojang.datafixers.kinds.K1;
import com.mojang.datafixers.kinds.OptionalBox;
import com.mojang.datafixers.util.Unit;
import java.util.Optional;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
public interface MemoryCondition<F extends K1, Value> {
    MemoryModuleType<Value> memory();
    MemoryStatus condition();
    MemoryAccessor<F, Value> createAccessor(Brain<?> brain, Optional<Value> value);
    public static record Registered<Value>(MemoryModuleType<Value> memory) implements MemoryCondition<OptionalBox.Mu, Value> {
        public MemoryStatus condition() {
            return MemoryStatus.REGISTERED;
        }
        public MemoryAccessor<OptionalBox.Mu, Value> createAccessor(Brain<?> brain, Optional<Value> value) {
            return new MemoryAccessor<>(brain, memory, OptionalBox.create(value));
        }
    }
    public static record Present<Value>(MemoryModuleType<Value> memory) implements MemoryCondition<IdF.Mu, Value> {
        public MemoryStatus condition() {
            return MemoryStatus.VALUE_PRESENT;
        }
        public MemoryAccessor<IdF.Mu, Value> createAccessor(Brain<?> brain, Optional<Value> value) {
            return value.isEmpty() ? null : new MemoryAccessor(brain, memory, IdF.create(value.get()));
        }
    }
    public static record Absent<Value>(MemoryModuleType<Value> memory) implements MemoryCondition<Const.Mu<Unit>, Value> {
        public MemoryStatus condition() {
            return MemoryStatus.VALUE_ABSENT;
        }
        public MemoryAccessor<Const.Mu<Unit>, Value> createAccessor(Brain<?> brain, Optional<Value> value) {
            return value.isPresent() ? null : new MemoryAccessor(brain, memory, Const.create(Unit.INSTANCE));


        }
    }
}
