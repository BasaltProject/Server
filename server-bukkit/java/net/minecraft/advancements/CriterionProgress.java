// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;
import io.netty.buffer.ByteBuf;
import java.time.Instant;
import java.util.Optional;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
public class CriterionProgress {
    public static final StreamCodec<ByteBuf, CriterionProgress> STREAM_CODEC = ByteBufCodecs.INSTANT.apply(ByteBufCodecs::optional)
        .map(obtained -> new CriterionProgress(obtained.orElse(null)), progress -> Optional.ofNullable(progress.obtained));
    private Instant obtained;
    public CriterionProgress() {}
    public CriterionProgress(Instant obtained) {
        this.obtained = obtained;
    }
    public boolean isDone() {
        return obtained != null;
    }
    public void grant() {
        obtained = Instant.now();
    }
    public void revoke() {
        obtained = null;
    }
    public Instant getObtained() {
        return obtained;
    }
    @Override
    public String toString() {
        return "CriterionProgress{obtained=" + (obtained == null ? "false" : obtained) + "}";
    }
}
