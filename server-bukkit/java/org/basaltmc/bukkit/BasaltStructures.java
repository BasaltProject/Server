package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.generator.structure.GeneratedStructure;
import org.bukkit.generator.structure.Structure;
import org.bukkit.generator.structure.StructurePiece;
import org.bukkit.persistence.PersistentDataContainer;
import org.bukkit.util.BoundingBox;

final class BasaltStructures {
    private BasaltStructures() {}

    private static BoundingBox box(String text) {
        String[] n = text.split(",");
        return new BoundingBox(Integer.parseInt(n[0]), Integer.parseInt(n[1]), Integer.parseInt(n[2]),
            Integer.parseInt(n[3]) + 1, Integer.parseInt(n[4]) + 1, Integer.parseInt(n[5]) + 1);
    }

    private record Piece(BoundingBox box) implements StructurePiece {
        @Override
        public BoundingBox getBoundingBox() {
            return box.clone();
        }
    }

    private static final class Start implements GeneratedStructure {
        private final BasaltWorld world;
        private final Structure structure;
        private final BoundingBox box;
        private final List<StructurePiece> pieces = new ArrayList<>();
        private final int chunkX;
        private final int chunkZ;

        Start(BasaltWorld world, String row) {
            this.world = world;
            String[] parts = row.split("\\|", -1);
            this.structure = Registry.STRUCTURE.get(NamespacedKey.fromString(parts[0]));
            this.chunkX = Integer.parseInt(parts[1]);
            this.chunkZ = Integer.parseInt(parts[2]);
            this.box = box(parts[3]);
            for (String piece : parts[4].isEmpty() ? new String[0] : parts[4].split(";")) {
                pieces.add(new Piece(box(piece)));
            }
        }

        @Override
        public BoundingBox getBoundingBox() {
            return box.clone();
        }

        @Override
        public Structure getStructure() {
            return structure;
        }

        @Override
        public Collection<StructurePiece> getPieces() {
            return List.copyOf(pieces);
        }

        @Override
        public PersistentDataContainer getPersistentDataContainer() {
            return org.basaltmc.bukkit.persistence.PdcStores.chunk(world.getName(), chunkX, chunkZ);
        }
    }

    static List<GeneratedStructure> in(BasaltWorld world, int chunkX, int chunkZ, Structure only) {
        List<GeneratedStructure> out = new ArrayList<>();
        String[] rows = WorldEnvNatives.structureStarts(world.getName(), chunkX, chunkZ);
        for (String row : rows == null ? new String[0] : rows) {
            Start start = new Start(world, row);
            if (start.structure != null && (only == null || only.getKey().equals(start.structure.getKey()))) {
                out.add(start);
            }
        }
        return out;
    }
}
