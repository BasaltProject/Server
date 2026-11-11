// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import it.unimi.dsi.fastutil.longs.LongOpenHashSet;
import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import java.util.function.BiConsumer;
import net.minecraft.server.level.ChunkLevel;
import net.minecraft.server.level.ChunkHolder;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.world.level.chunk.status.ChunkStatus;
import net.minecraft.server.level.FullChunkStatus;
import java.util.ArrayList;
import java.util.Iterator;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.longs.Long2ObjectOpenHashMap;
import it.unimi.dsi.fastutil.longs.LongSet;
import java.util.List;
import net.minecraft.server.level.ChunkMap;
import net.minecraft.server.level.Ticket;
import net.minecraft.server.level.TicketType;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
public class TicketStorage extends SavedData {
    private static TicketStorage fromPacked(List<Pair<ChunkPos, Ticket>> packed) {
        Long2ObjectOpenHashMap<List<Ticket>> byChunk = new Long2ObjectOpenHashMap<>();
        for (Pair<ChunkPos, Ticket> entry : packed) {
            byChunk.computeIfAbsent(entry.getFirst().pack(), key -> new ObjectArrayList<>(4)).add(entry.getSecond());
        }
        return new TicketStorage(new Long2ObjectOpenHashMap<>(4), byChunk);
    }
    private List<Pair<ChunkPos, Ticket>> packTickets() {
        List<Pair<ChunkPos, Ticket>> out = new ArrayList<>();
        forEachTicket((pos, ticket) -> {
            if (ticket.getType().persist()) {
                out.add(new Pair<>(pos, ticket));
            }
        });
        return out;
    }
    private void forEachTicket(BiConsumer<ChunkPos, Ticket> sink) {
        for (Long2ObjectOpenHashMap<List<Ticket>> source : List.of(tickets, deactivatedTickets)) {
            source.forEach((key, group) -> group.forEach(ticket -> sink.accept(ChunkPos.unpack(key), ticket)));
        }
    }
    private List<Ticket> groupFor(long key) {
        return tickets.computeIfAbsent(key, k -> new ObjectArrayList<>(4));
    }
    private static boolean sameKind(Ticket a, Ticket b) {
        return a.getType() == b.getType() && a.getTicketLevel() == b.getTicketLevel();
    }
    private static int levelOf(List<Ticket> group, boolean simulation) {
        Ticket best = lowest(group, simulation);
        return best == null ? ChunkLevel.MAX_LEVEL + 1 : best.getTicketLevel();
    }
    private static Ticket lowest(List<Ticket> group, boolean simulation) {
        Ticket best = null;
        if (group == null) {
            return null;
        }
        for (Ticket ticket : group) {
            boolean usable = simulation ? ticket.getType().doesSimulate() : ticket.getType().doesLoad();
            if (usable && (best == null || ticket.getTicketLevel() < best.getTicketLevel())) {
                best = ticket;
            }
        }
        return best;
    }
    private void refreshForced() {
        LongSet found = new LongOpenHashSet();
        tickets.forEach((key, group) -> {
            if (group.stream().anyMatch(ticket -> ticket.getType().equals(TicketType.FORCED))) {
                found.add((long) key);
            }
        });
        forced = found;
    }
    private boolean canExpire(ChunkMap chunkMap, Ticket ticket, long chunkPos) {
        if (!ticket.getType().hasTimeout()) {
            return false;
        }
        if (ticket.getType().canExpireIfUnloaded()) {
            return true;
        }
        ChunkHolder holder = chunkMap.getUpdatingChunkIfPresent(chunkPos);
        return holder == null || holder.isReadyForSaving();
    }
    private static final Codec<Pair<ChunkPos, Ticket>> kEntry = Codec.mapPair(ChunkPos.CODEC.fieldOf("chunk_pos"), Ticket.CODEC).codec();
    private final Long2ObjectOpenHashMap<List<Ticket>> tickets;
    private final Long2ObjectOpenHashMap<List<Ticket>> deactivatedTickets;
    private LongSet forced = new LongOpenHashSet();
    private ChunkUpdated loadingListener;
    private ChunkUpdated simulationListener;
    public static final Codec<TicketStorage> CODEC = RecordCodecBuilder.create(i -> i.group(
    kEntry.listOf().optionalFieldOf("tickets", List.of()).forGetter(TicketStorage::packTickets)).apply(i, TicketStorage::fromPacked));
    public static final SavedDataType<TicketStorage> TYPE = new SavedDataType<>(Identifier.withDefaultNamespace("chunk_tickets"), TicketStorage::new, CODEC, DataFixTypes.SAVED_DATA_FORCED_CHUNKS);
    private TicketStorage(Long2ObjectOpenHashMap<List<Ticket>> tickets, Long2ObjectOpenHashMap<List<Ticket>> deactivatedTickets) {
        this.tickets = tickets;
        this.deactivatedTickets = deactivatedTickets;
        refreshForced();
    }
    public TicketStorage() {
        this(new Long2ObjectOpenHashMap<>(4), new Long2ObjectOpenHashMap<>());
    }
    public void activateAllDeactivatedTickets() {
        deactivatedTickets.forEach((key, group) -> group.forEach(ticket -> addTicket((long) key, ticket)));
        deactivatedTickets.clear();
    }
    public void setLoadingChunkUpdatedListener(ChunkUpdated loadingChunkUpdatedListener) {
        loadingListener = loadingChunkUpdatedListener;
    }
    public void setSimulationChunkUpdatedListener(ChunkUpdated simulationChunkUpdatedListener) {
        simulationListener = simulationChunkUpdatedListener;
    }
    public boolean hasTickets() {
        return !tickets.isEmpty();
    }
    public boolean shouldKeepDimensionActive() {
        return tickets.values().stream().anyMatch(group -> group.stream().anyMatch(ticket -> ticket.getType().shouldKeepDimensionActive()));
    }
    public List<Ticket> getTickets(long key) {
        return tickets.getOrDefault(key, List.of());
    }
    public void addTicketWithRadius(TicketType type, ChunkPos chunkPos, int radius) {
        addTicket(chunkPos.pack(), new Ticket(type, ChunkLevel.byStatus(FullChunkStatus.FULL) - radius));
    }
    public void addTicket(Ticket ticket, ChunkPos chunkPos) {
        addTicket(chunkPos.pack(), ticket);
    }
    public boolean addTicket(long key, Ticket ticket) {
        List<Ticket> group = groupFor(key);
        for (Ticket present : group) {
            if (sameKind(ticket, present)) {
                present.resetTicksLeft();
                setDirty();
                return false;
            }
        }
        int wasSimulating = levelOf(group, true);
        int wasLoading = levelOf(group, false);
        group.add(ticket);
        if (ticket.getType().doesSimulate() && ticket.getTicketLevel() < wasSimulating && simulationListener != null) {
            simulationListener.update(key, ticket.getTicketLevel(), true);
        }
        if (ticket.getType().doesLoad() && ticket.getTicketLevel() < wasLoading && loadingListener != null) {
            loadingListener.update(key, ticket.getTicketLevel(), true);
        }
        if (ticket.getType().equals(TicketType.FORCED)) {
            forced.add(key);
        }
        setDirty();
        return true;
    }
    public int getTicketLevelAt(long key, boolean simulation) {
        return levelOf(getTickets(key), simulation);
    }
    public void removeTicketWithRadius(TicketType type, ChunkPos chunkPos, int radius) {
        removeTicket(chunkPos.pack(), new Ticket(type, ChunkLevel.byStatus(FullChunkStatus.FULL) - radius));
    }
    public void removeTicket(Ticket ticket, ChunkPos chunkPos) {
        removeTicket(chunkPos.pack(), ticket);
    }
    public boolean removeTicket(long key, Ticket ticket) {
        List<Ticket> group = tickets.get(key);
        if (group == null) {
            return false;
        }
        Ticket match = group.stream().filter(present -> sameKind(ticket, present)).findFirst().orElse(null);
        if (match == null) {
            return false;
        }
        group.remove(match);
        if (group.isEmpty()) {
            tickets.remove(key);
        }
        if (ticket.getType().doesSimulate() && simulationListener != null) {
            simulationListener.update(key, levelOf(group, true), false);
        }
        if (ticket.getType().doesLoad() && loadingListener != null) {
            loadingListener.update(key, levelOf(group, false), false);
        }
        if (ticket.getType().equals(TicketType.FORCED)) {
            refreshForced();
        }
        setDirty();
        return true;
    }
    public String getTicketDebugString(long key, boolean simulation) {
        Ticket best = lowest(getTickets(key), simulation);
        return best == null ? "no_ticket" : best.toString();
    }
    public void purgeStaleTickets(ChunkMap chunkMap) {
        removeTicketIf((ticket, chunkPos) -> {
            if (!canExpire(chunkMap, ticket, chunkPos)) {
                return false;
            }
            ticket.decreaseTicksLeft();
            return ticket.isTimedOut();
        }, null);
        setDirty();
    }
    public void deactivateTicketsOnClosing() {
        removeTicketIf((ticket, chunkPos) -> ticket.getType() != TicketType.UNKNOWN, deactivatedTickets);
    }
    public void removeTicketIf(TicketPredicate predicate, Long2ObjectOpenHashMap<List<Ticket>> removedTickets) {
        boolean forcedGone = false;
        var groups = tickets.long2ObjectEntrySet().fastIterator();
        while (groups.hasNext()) {
            var entry = groups.next();
            long chunkPos = entry.getLongKey();
            List<Ticket> group = entry.getValue();
            boolean loadingGone = false;
            boolean simulationGone = false;
            for (Iterator<Ticket> it = group.iterator(); it.hasNext();) {
                Ticket ticket = it.next();
                if (!predicate.test(ticket, chunkPos)) {
                    continue;
                }
                if (removedTickets != null) {
                    removedTickets.computeIfAbsent(chunkPos, k -> new ObjectArrayList<>(group.size())).add(ticket);
                }
                it.remove();
                loadingGone |= ticket.getType().doesLoad();
                simulationGone |= ticket.getType().doesSimulate();
                forcedGone |= ticket.getType().equals(TicketType.FORCED);
            }
            if (!loadingGone && !simulationGone) {
                continue;
            }
            if (loadingGone && loadingListener != null) {
                loadingListener.update(chunkPos, levelOf(group, false), false);
            }
            if (simulationGone && simulationListener != null) {
                simulationListener.update(chunkPos, levelOf(group, true), false);
            }
            setDirty();
            if (group.isEmpty()) {
                groups.remove();
            }
        }
        if (forcedGone) {
            refreshForced();
        }
    }
    public void replaceTicketLevelOfType(int newLevel, TicketType ticketType) {
        List<Pair<Ticket, Long>> affected = new ArrayList<>();
        tickets.forEach((key, group) -> group.stream().filter(ticket -> ticket.getType() == ticketType).forEach(ticket -> affected.add(Pair.of(ticket, (long) key))));
        for (Pair<Ticket, Long> hit : affected) {
            removeTicket(hit.getSecond(), hit.getFirst());
            addTicket(hit.getSecond(), new Ticket(hit.getFirst().getType(), newLevel));
        }
    }
    public boolean updateChunkForced(ChunkPos chunkPos, boolean forced) {
        Ticket ticket = new Ticket(TicketType.FORCED, ChunkMap.FORCED_TICKET_LEVEL);
        return forced ? addTicket(chunkPos.pack(), ticket) : removeTicket(chunkPos.pack(), ticket);
    }
    public LongSet getForceLoadedChunks() {
        return this.forced;

    }
    public interface ChunkUpdated {
        void update(long node, int newLevelFrom, boolean onlyDecreased);
    }
    public interface TicketPredicate {
        boolean test(Ticket ticket, long chunkPos);
    }
}
