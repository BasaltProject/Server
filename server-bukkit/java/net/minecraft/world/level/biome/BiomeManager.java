// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.QuartPos;
import net.minecraft.util.LinearCongruentialGenerator;
import net.minecraft.util.Mth;

public class BiomeManager {
    public static final int CHUNK_CENTER_QUART = QuartPos.fromBlock(8);
    private final BiomeResolver noiseBiomeSource;
    private final long biomeZoomSeed;

    protected BiomeManager() {
        this(null, 0L);
    }

    public BiomeManager(BiomeResolver noiseBiomeSource, long seed) {
        this.noiseBiomeSource = noiseBiomeSource;
        this.biomeZoomSeed = seed;
    }

    public static long obfuscateSeed(long seed) {
        try {
            byte[] hash = MessageDigest.getInstance("SHA-256").digest(ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN).putLong(seed).array());
            return ByteBuffer.wrap(hash, 0, 8).order(ByteOrder.LITTLE_ENDIAN).getLong();
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        }
    }

    public BiomeManager withDifferentSource(BiomeResolver biomeSource) {
        return new BiomeManager(biomeSource, biomeZoomSeed);
    }

    public Holder<Biome> getBiome(BlockPos pos) {
        return getBiome(pos.getX(), pos.getY(), pos.getZ());
    }

    public Holder<Biome> getBiome(int x, int y, int z) {
        int ax = x - 2;
        int ay = y - 2;
        int az = z - 2;
        int px = ax >> 2;
        int py = ay >> 2;
        int pz = az >> 2;
        double fx = (ax & 3) / 4.0;
        double fy = (ay & 3) / 4.0;
        double fz = (az & 3) / 4.0;
        int best = 0;
        double bestDistance = Double.POSITIVE_INFINITY;
        for (int i = 0; i < 8; i++) {
            boolean lowX = (i & 4) == 0;
            boolean lowY = (i & 2) == 0;
            boolean lowZ = (i & 1) == 0;
            double d = fiddledDistance(biomeZoomSeed, lowX ? px : px + 1, lowY ? py : py + 1, lowZ ? pz : pz + 1,
                lowX ? fx : fx - 1.0, lowY ? fy : fy - 1.0, lowZ ? fz : fz - 1.0);
            if (bestDistance > d) {
                best = i;
                bestDistance = d;
            }
        }
        return noiseBiomeSource.getNoiseBiome((best & 4) == 0 ? px : px + 1, (best & 2) == 0 ? py : py + 1, (best & 1) == 0 ? pz : pz + 1);
    }

    public Holder<Biome> getNoiseBiomeAtPosition(double x, double y, double z) {
        return getNoiseBiomeAtQuart(QuartPos.fromBlock(Mth.floor(x)), QuartPos.fromBlock(Mth.floor(y)), QuartPos.fromBlock(Mth.floor(z)));
    }

    public Holder<Biome> getNoiseBiomeAtPosition(BlockPos blockPos) {
        return getNoiseBiomeAtQuart(QuartPos.fromBlock(blockPos.getX()), QuartPos.fromBlock(blockPos.getY()), QuartPos.fromBlock(blockPos.getZ()));
    }

    public Holder<Biome> getNoiseBiomeAtQuart(int quartX, int quartY, int quartZ) {
        return noiseBiomeSource.getNoiseBiome(quartX, quartY, quartZ);
    }

    private static double fiddledDistance(long seed, int cx, int cy, int cz, double dx, double dy, double dz) {
        long r = LinearCongruentialGenerator.next(seed, cx);
        r = LinearCongruentialGenerator.next(r, cy);
        r = LinearCongruentialGenerator.next(r, cz);
        r = LinearCongruentialGenerator.next(r, cx);
        r = LinearCongruentialGenerator.next(r, cy);
        r = LinearCongruentialGenerator.next(r, cz);
        double fx = fiddle(r);
        r = LinearCongruentialGenerator.next(r, seed);
        double fy = fiddle(r);
        r = LinearCongruentialGenerator.next(r, seed);
        double fz = fiddle(r);
        return Mth.square(dz + fz) + Mth.square(dy + fy) + Mth.square(dx + fx);
    }

    private static double fiddle(long r) {
        return (Math.floorMod(r >> 24, 1024) / 1024.0 - 0.5) * 0.9;
    }
}
