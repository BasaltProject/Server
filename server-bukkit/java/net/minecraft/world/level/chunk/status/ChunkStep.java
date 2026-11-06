// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk.status;

import com.google.common.collect.ImmutableList;
import java.util.Arrays;
import java.util.concurrent.CompletableFuture;
import net.minecraft.server.level.GenerationChunkHolder;
import net.minecraft.util.StaticCache2D;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ProtoChunk;

public record ChunkStep(ChunkStatus targetStatus, ChunkDependencies directDependencies, ChunkDependencies accumulatedDependencies, int blockStateWriteRadius, ChunkStatusTask task) {
    public int getAccumulatedRadiusOf(ChunkStatus status) {
        return status == targetStatus ? 0 : accumulatedDependencies.getRadiusOf(status);
    }

    public CompletableFuture<ChunkAccess> apply(WorldGenContext context, StaticCache2D<GenerationChunkHolder> cache, ChunkAccess chunk) {
        CompletableFuture<ChunkAccess> work = task.doWork(context, this, cache, chunk);
        if (!chunk.getPersistedStatus().isBefore(targetStatus)) {
            return work;
        }
        return work.thenApply(done -> {
            if (done instanceof ProtoChunk proto && proto.getPersistedStatus().isBefore(targetStatus)) {
                proto.setPersistedStatus(targetStatus);
            }
            return done;
        });
    }

    public static class Builder {
        private final ChunkStatus status;
        private final ChunkStep parent;
        private ChunkStatus[] direct;
        private int blockStateWriteRadius = -1;
        private ChunkStatusTask task = ChunkStatusTasks::passThrough;

        protected Builder(ChunkStatus status) {
            if (status.getParent() != status) {
                throw new IllegalArgumentException("Not starting with the first status: " + status);
            }
            this.status = status;
            this.parent = null;
            this.direct = new ChunkStatus[0];
        }

        protected Builder(ChunkStatus status, ChunkStep parent) {
            if (parent.targetStatus.getIndex() != status.getIndex() - 1) {
                throw new IllegalArgumentException("Out of order status: " + status);
            }
            this.status = status;
            this.parent = parent;
            this.direct = new ChunkStatus[] {parent.targetStatus};
        }

        public Builder addRequirement(ChunkStatus required, int radius) {
            if (required.isOrAfter(status)) {
                throw new IllegalArgumentException("Status " + required + " can not be required by " + status);
            }
            ChunkStatus[] previous = direct;
            int length = radius + 1;
            if (length > previous.length) {
                direct = new ChunkStatus[length];
                Arrays.fill(direct, required);
            }
            for (int i = 0; i < Math.min(length, previous.length); i++) {
                direct[i] = ChunkStatus.max(previous[i], required);
            }
            return this;
        }

        public Builder blockStateWriteRadius(int radius) {
            this.blockStateWriteRadius = radius;
            return this;
        }

        public Builder setTask(ChunkStatusTask task) {
            this.task = task;
            return this;
        }

        public ChunkStep build() {
            return new ChunkStep(status, new ChunkDependencies(ImmutableList.copyOf(direct)),
                new ChunkDependencies(ImmutableList.copyOf(accumulated())), blockStateWriteRadius, task);
        }

        private ChunkStatus[] accumulated() {
            if (parent == null) {
                return direct;
            }
            int offset = radiusOfParent();
            ChunkDependencies inherited = parent.accumulatedDependencies;
            ChunkStatus[] merged = new ChunkStatus[Math.max(offset + inherited.size(), direct.length)];
            for (int distance = 0; distance < merged.length; distance++) {
                int inParent = distance - offset;
                boolean inherits = inParent >= 0 && inParent < inherited.size();
                if (!inherits) {
                    merged[distance] = direct[distance];
                } else if (distance >= direct.length) {
                    merged[distance] = inherited.get(inParent);
                } else {
                    merged[distance] = ChunkStatus.max(direct[distance], inherited.get(inParent));
                }
            }
            return merged;
        }

        private int radiusOfParent() {
            for (int i = direct.length - 1; i >= 0; i--) {
                if (direct[i].isOrAfter(parent.targetStatus)) {
                    return i;
                }
            }
            return 0;
        }
    }
}
