// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.status;

import com.google.common.collect.ImmutableList;
import java.util.function.UnaryOperator;

public record ChunkPyramid(Object steps) {
    public static ChunkPyramid LOADING_PYRAMID = new ChunkPyramid(null);

    public ChunkStep getStepTo(ChunkStatus status) {
        return new ChunkStep(status, null, null, 0, null);
    }
    public static final ChunkPyramid GENERATION_PYRAMID = null;
    public static final int MAX_CHUNK_COORDINATE_VALUE = 0;
    public static class Builder {
        public ChunkPyramid build() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkPyramid.Builder#build");
        }
        public Builder step(ChunkStatus status, UnaryOperator<ChunkStep.Builder> operator) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.chunk.status.ChunkPyramid.Builder#step");
        }
    }
}
