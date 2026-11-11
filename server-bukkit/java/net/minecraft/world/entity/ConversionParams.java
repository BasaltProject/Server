// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.world.scores.PlayerTeam;
public record ConversionParams(ConversionType type, boolean keepEquipment, boolean preserveCanPickUpLoot, PlayerTeam team) {
    public static ConversionParams single(Mob mob, boolean keepEquipment, boolean preserveCanPickUpLoot) {
        return new ConversionParams(ConversionType.SINGLE, keepEquipment, preserveCanPickUpLoot, mob.getTeam());

    }
    public interface AfterConversion<T extends Mob> {
        void finalizeConversion(T mob);
    }
}
