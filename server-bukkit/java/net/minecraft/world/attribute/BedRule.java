// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;

import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;

public record BedRule(Rule canSleep, Rule canSetSpawn, boolean destroyOnUse, boolean destroyOnLeave, Optional<Component> errorMessage) {
    public static final BedRule CAN_SLEEP_WHEN_DARK = new BedRule(Rule.WHEN_DARK, Rule.ALWAYS, false, false, Optional.of(Component.translatable("block.minecraft.bed.no_sleep")));
    public static final BedRule DESTROY_ON_USE = new BedRule(Rule.NEVER, Rule.NEVER, true, false, Optional.empty());
    public static final BedRule DESTROY_ON_LEAVE = new BedRule(Rule.WHEN_DARK, Rule.NEVER, false, true, Optional.of(Component.translatable("block.minecraft.bed.no_sleep")));
    public static final Codec<BedRule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(BedRule.class, "CODEC");

    public boolean canSleep(Level level) {
        return canSleep.test(level);
    }

    public boolean canSetSpawn(Level level) {
        return canSetSpawn.test(level);
    }

    public Player.BedSleepingProblem asProblem() {
        return new Player.BedSleepingProblem(errorMessage.orElse(null));
    }

    public enum Rule implements StringRepresentable {
        ALWAYS, WHEN_DARK, NEVER;
        public static final Codec<Rule> CODEC = StringRepresentable.fromEnum(Rule::values);

        public boolean test(Level level) {
            return this == ALWAYS || (this == WHEN_DARK && level.isDarkOutside());
        }

        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
}
