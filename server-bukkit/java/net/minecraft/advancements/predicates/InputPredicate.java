// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.world.entity.player.Input;
public record InputPredicate(Optional<Boolean> forward, Optional<Boolean> backward, Optional<Boolean> left, Optional<Boolean> right, Optional<Boolean> jump, Optional<Boolean> sneak, Optional<Boolean> sprint) {
    public static final Codec<InputPredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        Codec.BOOL.optionalFieldOf("forward").forGetter(InputPredicate::forward),
        Codec.BOOL.optionalFieldOf("backward").forGetter(InputPredicate::backward),
        Codec.BOOL.optionalFieldOf("left").forGetter(InputPredicate::left),
        Codec.BOOL.optionalFieldOf("right").forGetter(InputPredicate::right),
        Codec.BOOL.optionalFieldOf("jump").forGetter(InputPredicate::jump),
        Codec.BOOL.optionalFieldOf("sneak").forGetter(InputPredicate::sneak),
        Codec.BOOL.optionalFieldOf("sprint").forGetter(InputPredicate::sprint)).apply(i, InputPredicate::new));
    public boolean matches(Input input) {
        return agrees(forward, input.forward()) && agrees(backward, input.backward()) && agrees(left, input.left())
            && agrees(right, input.right()) && agrees(jump, input.jump()) && agrees(sneak, input.shift())
            && agrees(sprint, input.sprint());
    }
    private static boolean agrees(Optional<Boolean> wanted, boolean actual) {
        return wanted.isEmpty() || wanted.get() == actual;
    }
}
