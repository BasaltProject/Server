// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.phys.Vec3;
public interface EnvironmentAttributeReader {
    EnvironmentAttributeReader EMPTY = null;
    <Value> Value getDimensionValue(EnvironmentAttribute<Value> attribute);
    default <Value> Value getValue(EnvironmentAttribute<Value> attribute, BlockPos pos) {
        return getValue(attribute, Vec3.atCenterOf(pos));
    }
    default <Value> Value getValue(EnvironmentAttribute<Value> attribute, Vec3 pos) {
        return getValue(attribute, pos, null);
    }
    <Value> Value getValue(EnvironmentAttribute<Value> attribute, Vec3 pos, SpatialAttributeInterpolator biomeInterpolator);
    default <Value> Value getValue(LootContext context, EnvironmentAttribute<Value> attribute) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.attribute.EnvironmentAttributeReader#getValue");
    }
}
