// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Map;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class ScoreboardSaveData extends SavedData {
    private Packed data;
    public static final SavedDataType<ScoreboardSaveData> TYPE = new SavedDataType<>(net.minecraft.resources.Identifier.withDefaultNamespace("scoreboard"), () -> new ScoreboardSaveData(Packed.EMPTY), Packed.CODEC.xmap(ScoreboardSaveData::new, ScoreboardSaveData::getData), net.minecraft.util.datafix.DataFixTypes.SAVED_DATA_SCOREBOARD);
    private ScoreboardSaveData() {
        this(Packed.EMPTY);
    }
    public ScoreboardSaveData(Packed data) {
        this.data = data;
    }
    public Packed getData() {
        return data;
    }
    public void setData(Packed data) {
        if (!data.equals(this.data)) {
            this.data = data;
            setDirty();
        }
    }
    public static record Packed(List<Objective.Packed> objectives, List<Scoreboard.PackedScore> scores, Map<DisplaySlot, String> displaySlots, List<PlayerTeam.Packed> teams) {
        public static final Packed EMPTY = new Packed(List.of(), List.of(), Map.of(), List.of());
        public static final Codec<Packed> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.scores.ScoreboardSaveData.Packed.class, "CODEC");
    }
}
