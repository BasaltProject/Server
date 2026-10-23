package org.basaltmc.bukkit.plugin;

import io.papermc.paper.plugin.loader.library.ClassPathLibrary;
import io.papermc.paper.plugin.loader.library.LibraryLoadingException;
import io.papermc.paper.plugin.loader.library.LibraryStore;
import io.papermc.paper.plugin.loader.library.impl.MavenLibraryResolver;
import java.util.ArrayList;
import java.util.List;
import org.apache.maven.repository.internal.MavenRepositorySystemUtils;
import org.eclipse.aether.DefaultRepositorySystemSession;
import org.eclipse.aether.RepositorySystem;
import org.eclipse.aether.collection.CollectRequest;
import org.eclipse.aether.connector.basic.BasicRepositoryConnectorFactory;
import org.eclipse.aether.graph.Dependency;
import org.eclipse.aether.impl.DefaultServiceLocator;
import org.eclipse.aether.repository.LocalRepository;
import org.eclipse.aether.repository.RemoteRepository;
import org.eclipse.aether.resolution.ArtifactResult;
import org.eclipse.aether.resolution.DependencyRequest;
import org.eclipse.aether.resolution.DependencyResolutionException;
import org.eclipse.aether.spi.connector.RepositoryConnectorFactory;
import org.eclipse.aether.spi.connector.transport.TransporterFactory;
import org.eclipse.aether.transport.http.HttpTransporterFactory;

/** Resolves Maven coordinates into libraries/; one process owns the folder, so the resolver takes no locks. */
public final class MavenLibraries implements ClassPathLibrary {
    private final RepositorySystem system;
    private final DefaultRepositorySystemSession session;
    private final List<RemoteRepository> repositories;
    private final List<Dependency> dependencies;

    public MavenLibraries(List<RemoteRepository> repositories, List<Dependency> dependencies) {
        DefaultServiceLocator locator = MavenRepositorySystemUtils.newServiceLocator();
        locator.addService(RepositoryConnectorFactory.class, BasicRepositoryConnectorFactory.class);
        locator.addService(TransporterFactory.class, HttpTransporterFactory.class);
        system = locator.getService(RepositorySystem.class);
        session = MavenRepositorySystemUtils.newSession();
        session.setSystemProperties(System.getProperties());
        session.setConfigProperty("aether.syncContext.named.factory", "noop");
        session.setChecksumPolicy("fail");
        session.setLocalRepositoryManager(system.newLocalRepositoryManager(session, new LocalRepository("libraries")));
        this.repositories = new ArrayList<>(repositories);
        this.dependencies = new ArrayList<>(dependencies);
    }

    /** The same request a plugin made of Paper's resolver, answered without its locks. */
    @SuppressWarnings("unchecked")
    public static ClassPathLibrary lockFree(ClassPathLibrary library) {
        if (!(library instanceof MavenLibraryResolver)) {
            return library;
        }
        try {
            java.lang.reflect.Field repositories = MavenLibraryResolver.class.getDeclaredField("repositories");
            java.lang.reflect.Field dependencies = MavenLibraryResolver.class.getDeclaredField("dependencies");
            repositories.setAccessible(true);
            dependencies.setAccessible(true);
            return new MavenLibraries((List<RemoteRepository>) repositories.get(library),
                (List<Dependency>) dependencies.get(library));
        } catch (ReflectiveOperationException | RuntimeException e) {
            return library;
        }
    }

    @Override
    public void register(LibraryStore store) throws LibraryLoadingException {
        CollectRequest collect =
            new CollectRequest((Dependency) null, dependencies, system.newResolutionRepositories(session, repositories));
        List<ArtifactResult> results;
        try {
            results = system.resolveDependencies(session, new DependencyRequest(collect, null)).getArtifactResults();
        } catch (DependencyResolutionException e) {
            throw new LibraryLoadingException("Error resolving libraries", e);
        }
        for (ArtifactResult result : results) {
            store.addLibrary(result.getArtifact().getFile().toPath());
        }
    }
}
