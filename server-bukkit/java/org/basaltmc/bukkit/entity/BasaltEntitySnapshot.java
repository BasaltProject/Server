package org.basaltmc.bukkit.entity;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.ListTag;
import org.basaltmc.bukkit.BasaltWorld;
import org.basaltmc.bukkit.Blocks;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.Location;
import org.bukkit.World;
import org.bukkit.entity.Entity;
import org.bukkit.entity.EntitySnapshot;
import org.bukkit.entity.EntityType;

/** A frozen copy of an entity: its type and the NBT the server saves for it. */
public final class BasaltEntitySnapshot implements EntitySnapshot {
    private final EntityType type;
    private final byte[] nbt;

    private BasaltEntitySnapshot(EntityType type, byte[] nbt) {
        this.type = type;
        this.nbt = nbt;
    }

    public static EntitySnapshot of(EntityCore core) {
        byte[] saved = core.nbt("");
        return saved == null ? null : new BasaltEntitySnapshot(core.type, saved);
    }

    @Override
    public EntityType getEntityType() {
        return type;
    }

    @Override
    public Entity createEntity(World world) {
        CompoundTag tag = NbtCodec.fromBytes(nbt);
        if (tag == null || !(world instanceof BasaltWorld basalt)) {
            return null;
        }
        tag.remove("UUID");
        Blocks.flushAll();
        int id = WorldNatives.spawnEntityNbt(world.getName(), NbtCodec.toBytes(tag), "spawner_egg");
        return id < 0 ? null : basalt.entity(id);
    }

    @Override
    public Entity createEntity(Location location) {
        CompoundTag tag = NbtCodec.fromBytes(nbt);
        if (tag == null || !(location.getWorld() instanceof BasaltWorld basalt)) {
            return null;
        }
        tag.remove("UUID");
        ListTag position = new ListTag();
        position.add(DoubleTag.valueOf(location.getX()));
        position.add(DoubleTag.valueOf(location.getY()));
        position.add(DoubleTag.valueOf(location.getZ()));
        tag.put("Pos", position);
        Blocks.flushAll();
        int id = WorldNatives.spawnEntityNbt(basalt.getName(), NbtCodec.toBytes(tag), "spawner_egg");
        return id < 0 ? null : basalt.entity(id);
    }

    @Override
    public String getAsString() {
        return NbtCodec.fromBytes(nbt).toString();
    }
}
