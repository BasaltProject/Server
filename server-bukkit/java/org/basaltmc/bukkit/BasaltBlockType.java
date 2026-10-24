package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.function.Consumer;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.block.BlockType;
import org.bukkit.block.data.BlockData;
import org.bukkit.inventory.ItemType;
import org.basaltmc.bukkit.gen.BlockTypeTypedBase;

@SuppressWarnings({"rawtypes", "unchecked"})
public final class BasaltBlockType extends BlockTypeTypedBase {
    private final int block;
    private final NamespacedKey key;

    BasaltBlockType(int block) {
        this.block = block;
        this.key = NamespacedKey.fromString(Registries.blockName(block));
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public BlockType.Typed typed() {
        return this;
    }

    @Override
    public BlockType.Typed typed(Class type) {
        if (!type.isAssignableFrom(getBlockDataClass())) {
            throw new IllegalArgumentException(getBlockDataClass() + " is not a " + type);
        }
        return this;
    }

    @Override
    public org.bukkit.Instrument getInstrument() {
        String name = RestNatives.blockInstrument(Registries.defaultState(block));
        return name == null ? org.bukkit.Instrument.PIANO : switch (name) {
            case "basedrum" -> org.bukkit.Instrument.BASS_DRUM;
            case "snare" -> org.bukkit.Instrument.SNARE_DRUM;
            case "hat" -> org.bukkit.Instrument.STICKS;
            case "bass" -> org.bukkit.Instrument.BASS_GUITAR;
            case "flute" -> org.bukkit.Instrument.FLUTE;
            case "bell" -> org.bukkit.Instrument.BELL;
            case "guitar" -> org.bukkit.Instrument.GUITAR;
            case "chime" -> org.bukkit.Instrument.CHIME;
            case "xylophone" -> org.bukkit.Instrument.XYLOPHONE;
            case "iron_xylophone" -> org.bukkit.Instrument.IRON_XYLOPHONE;
            case "cow_bell" -> org.bukkit.Instrument.COW_BELL;
            case "didgeridoo" -> org.bukkit.Instrument.DIDGERIDOO;
            case "bit" -> org.bukkit.Instrument.BIT;
            case "banjo" -> org.bukkit.Instrument.BANJO;
            case "pling" -> org.bukkit.Instrument.PLING;
            case "zombie" -> org.bukkit.Instrument.ZOMBIE;
            case "skeleton" -> org.bukkit.Instrument.SKELETON;
            case "creeper" -> org.bukkit.Instrument.CREEPER;
            case "dragon" -> org.bukkit.Instrument.DRAGON;
            case "wither_skeleton" -> org.bukkit.Instrument.WITHER_SKELETON;
            case "piglin" -> org.bukkit.Instrument.PIGLIN;
            case "custom_head" -> org.bukkit.Instrument.CUSTOM_HEAD;
            default -> org.bukkit.Instrument.PIANO;
        };
    }

    @Override
    public boolean hasItemType() {
        return Registries.materialOfBlock(block).isItem();
    }

    @Override
    public ItemType getItemType() {
        return Registries.materialOfBlock(block).asItemType();
    }

    @Override
    public Class getBlockDataClass() {
        Class<?> data = Registries.materialOfBlock(block).data;
        return BlockData.class.isAssignableFrom(data) ? data : BlockData.class;
    }

    @Override
    public BlockData createBlockData(Consumer consumer) {
        BlockData data = createBlockData();
        if (consumer != null) {
            consumer.accept(data);
        }
        return data;
    }

    @Override
    public BlockData createBlockData() {
        return BasaltBlockData.of(Registries.defaultState(block));
    }

    @Override
    public Collection createBlockDataStates() {
        Collection<BlockData> all = new ArrayList<>();
        int first = Registries.firstState(block);
        for (int i = 0; i < Registries.stateCount(block); i++) {
            all.add(BasaltBlockData.of(first + i));
        }
        return all;
    }

    @Override
    public BlockData createBlockData(String data) {
        BlockData parsed = BasaltBlockData.parse(data == null || data.startsWith("[")
            ? key + (data == null ? "" : data) : data);
        if (parsed == null || BasaltBlockData.stateOf(parsed) < 0) {
            throw new IllegalArgumentException("Could not parse data: " + data);
        }
        return parsed;
    }

    private int flags() {
        return WorldNatives.stateFlags(Registries.defaultState(block));
    }

    private double[] hardness() {
        double[] facts = WorldNatives.blockHardness(Registries.blockName(block));
        return facts == null ? new double[] {0, 0, 0} : facts;
    }

    @Override
    public boolean isSolid() {
        return (flags() & 1) != 0;
    }

    @Override
    public boolean isFlammable() {
        return BlockFacts.ignitedByLava(key.getKey());
    }

    @Override
    public boolean isBurnable() {
        double[] info = WorldNatives.blockInfo(Registries.defaultState(block));
        return info != null && info[5] > 0;
    }

    @Override
    public boolean isOccluding() {
        return (flags() & 2) != 0;
    }

    @Override
    public boolean hasGravity() {
        String path = key.getKey();
        return path.equals("sand") || path.equals("red_sand") || path.equals("gravel") || path.equals("anvil")
            || path.endsWith("concrete_powder") || path.equals("dragon_egg");
    }

    @Override
    public boolean isInteractable() {
        return BlockFacts.interactable(key.getKey());
    }

    @Override
    public float getHardness() {
        return (float) hardness()[0];
    }

    @Override
    public float getBlastResistance() {
        return (float) hardness()[1];
    }

    @Override
    public float getSlipperiness() {
        double[] info = WorldNatives.blockInfo(Registries.defaultState(block));
        return info == null ? 0.6f : (float) info[0];
    }

    @Override
    public boolean isAir() {
        return (flags() & 256) != 0;
    }

    @Override
    public boolean isEnabledByFeature(org.bukkit.World world) {
        return true;
    }

    @Override
    public Material asMaterial() {
        return Registries.materialOfBlock(block);
    }

    @Override
    public String getTranslationKey() {
        return "block." + key.getNamespace() + "." + key.getKey();
    }

    @Override
    public String translationKey() {
        return getTranslationKey();
    }

    @Override
    public boolean hasCollision() {
        return (flags() & 32) != 0;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltBlockType type && type.block == block;
    }

    @Override
    public int hashCode() {
        return block;
    }

    @Override
    public String toString() {
        return key.toString();
    }
}
