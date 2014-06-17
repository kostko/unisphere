import networkx as nx

from .. import exceptions


def load(topology, arguments, filename):
  """
  Loads a topology from a file.
  """

  # Determine file format
  fmt = topology.settings['format']
  in_filename = topology.settings['filename']

  if fmt == "edges":
    graph = nx.read_edgelist(in_filename)
  elif fmt == "gml":
    graph = nx.read_gml(in_filename)
  else:
    raise exceptions.ImproperlyConfigured("Topology format '%s' is not supported!" % fmt)

  # Convert to GraphML and write to output file
  nx.write_graphml(graph, filename)
