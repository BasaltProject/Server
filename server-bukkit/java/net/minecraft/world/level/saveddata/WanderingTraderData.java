// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata;
import com.mojang.serialization.Codec;
public class WanderingTraderData extends SavedData {
    private int spawnDelay;
    private int spawnChance;
    public static final Codec<WanderingTraderData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.saveddata.WanderingTraderData.class, "CODEC");
    public static final SavedDataType<WanderingTraderData> TYPE = new SavedDataType<>(net.minecraft.resources.Identifier.withDefaultNamespace("wandering_trader"), WanderingTraderData::new, CODEC, net.minecraft.util.datafix.DataFixTypes.SAVED_DATA_WANDERING_TRADER);
    public WanderingTraderData() {
        this(24000, 25);
    }
    public WanderingTraderData(int spawnDelay, int spawnChance) {
        this.spawnDelay = spawnDelay;
        this.spawnChance = spawnChance;
    }
    public int spawnDelay() {
        return spawnDelay;
    }
    public void setSpawnDelay(int spawnDelay) {
        if (this.spawnDelay != spawnDelay) {
            this.spawnDelay = spawnDelay;
            setDirty(true);
        }
    }
    public int spawnChance() {
        return spawnChance;
    }
    public void setSpawnChance(int spawnChance) {
        if (this.spawnChance != spawnChance) {
            this.spawnChance = spawnChance;
            setDirty(true);
        }
    }
}
