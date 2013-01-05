// vim: si:ts=2:sw=2:expandtab:smartindent
// standard includes
#include <iostream>
#include <fstream>
#include <cassert>

//#include <map>

#include <CGAL/Simple_cartesian.h>

#include <CGAL/Apollonius_graph_2.h>
#include <CGAL/Triangulation_data_structure_2.h>
#include <CGAL/Apollonius_graph_vertex_base_2.h>
#include <CGAL/Triangulation_face_base_2.h>
#include <CGAL/Apollonius_graph_filtered_traits_2.h>

#include <CGAL/Voronoi_diagram_2.h>
#include <CGAL/Apollonius_graph_adaptation_traits_2.h>
#include <CGAL/Apollonius_graph_adaptation_policies_2.h>

// Qt includes

#include <CGAL/Qt/Converter.h>
#include <CGAL/Qt/GraphicsViewNavigation.h>
#include <CGAL/Qt/utility.h>
#include <QtGui/QApplication>
#include <QtGui/QTextEdit>
#include <QtGui/QGraphicsView>
#include <QLineF>
#include <QRectF>

typedef CGAL::Simple_cartesian<double>                Rep;

typedef CGAL::Apollonius_graph_filtered_traits_2<Rep> Traits;
// the second template argument is whether to store the hidden sites.
typedef CGAL::Apollonius_graph_vertex_base_2<Traits,false>  
                                                      Vb;
typedef CGAL::Triangulation_face_base_2<Traits>       Fb;
typedef CGAL::Triangulation_data_structure_2<Vb,Fb>   Agds;
typedef CGAL::Apollonius_graph_2<Traits,Agds>         Apollonius_graph;

typedef Agds::Face                                    Face;
typedef Agds::Face_handle                             Face_handle;
typedef Agds::Vertex                                  Vertex;
typedef Agds::Vertex_handle                           Vertex_handle;

typedef Agds::Face_iterator                           All_faces_iterator;
typedef Agds::Edge_iterator                           All_edges_iterator;
typedef Agds::Vertex_iterator                         All_vertices_iterator;
typedef Apollonius_graph::Finite_faces_iterator       Finite_faces_iterator;
typedef Apollonius_graph::Finite_edges_iterator       Finite_edges_iterator;
typedef Apollonius_graph::Finite_vertices_iterator    Finite_vertices_iterator;
typedef Agds::Face_circulator                         Face_circulator;
typedef Agds::Edge_circulator                         Edge_circulator;

typedef Traits::Object_2                              Object_2;
typedef Traits::Site_2                                Site_2;
typedef Traits::Point_2                               Point_2;
typedef Traits::Line_2                                Line_2;
typedef Traits::Segment_2                             Segment_2;
typedef Traits::Ray_2                                 Ray_2;
typedef Rep::Iso_rectangle_2                          Iso_rectangle_2;

typedef CGAL::Apollonius_graph_adaptation_traits_2<Apollonius_graph>
                                                      AT;
typedef CGAL::Apollonius_graph_degeneracy_removal_policy_2<Apollonius_graph>
                                                      AP;

typedef CGAL::Voronoi_diagram_2<Apollonius_graph,AT,AP>
                                                      Voronoi_diagram;
typedef Voronoi_diagram::Face_iterator                VoronoiFace_iterator;
typedef Voronoi_diagram::Face                         VoronoiFace;
typedef Voronoi_diagram::Vertex                       VoronoiVertex;
typedef Voronoi_diagram::Vertex_handle                VoronoiVertex_handle;
typedef Voronoi_diagram::Halfedge                     VoronoiHalfedge;
typedef Voronoi_diagram::Halfedge_handle              VoronoiHalfedge_handle;

// forward declarations
bool containsPoint(Iso_rectangle_2 rect, Point_2 point);
Iso_rectangle_2 boundingBox(std::vector<Site_2> sites);
Iso_rectangle_2 extend(Iso_rectangle_2 rect, double amount);

// TODO: use a custom type that extends Site_2. We need this to store the ids
// in the sites so we can later access them. 
// At the moment we don't know how to do that. Can something similar to this
// http://www.cgal.org/Manual/latest/doc_html/cgal_manual/Triangulation_2/Chapter_main.html#Section_37.11
// be done with our Site_2 type?

// An alternative may be to store 
// extra data in a separate data type T and store instances of that in a map
// that maps sites to instances of T.

int main(int argc , char* argv[])
{
  if (argc != 4) {
    std::cout << "usage: test <input file> <output image> <output folder>" << std::endl;
    exit(1);
  }

  char* input = argv[1];
  char* output = argv[2];
  char* outdir = argv[3];

  std::ifstream ifs(input);
  assert( ifs );

  Apollonius_graph ag;

  // we use a ScalingFactor(SF) here to stretch input values at the 
  // beginning, and divide by SF in the end. This is used because the 
  // point-generation of the hyperbola class is using some arbitrary
  // internal decision thresholds to decide how many points to generate for
  // a certain part of the curve. Rule of thumb is: the higher SF the more
  // detail is used in approximation of the hyperbolas.
  double SF = 4000;

  // collect the sites here
  std::vector<Site_2> sites;
  // a number of artificial sites
  std::vector<Site_2> artificialSites;
  // a site vector iterator
  std::vector<Site_2>::iterator itr;

  // read in sites from input file
  std::string line;
  while (std::getline(ifs, line)) {
    std::istringstream iss(line);
    std::string type;
    int id;
    double  lat, lon;
    iss >> id; // TODO: store id in a custom Site-type
    iss >> lon;
    iss >> lat;
    iss >> type;
    double weight = 0;
    if (type == "city") {
      weight = 0.1;
    } else if (type == "town") {
      weight = 0.04;
    } else if (type == "village") {
      weight = 0.01;
    }
    Site_2 site(Point_2(lon*SF, lat*SF), weight*SF);
    sites.push_back(site);
  }

  // print sites
  for (itr = sites.begin(); itr != sites.end(); ++itr) {
    Site_2 site = *itr;
    Point_2 point = site.point();
    std::cout << "site: " << point << std::endl;
  }

  // calculate bounding box of all input sites (and extend it a little).
  // Extension is important, because we later add artificial sites which are
  // actually mirrored on the bounds of this rectangle. If we did not extend
  // some points would lie on the boundary of the bounding box and so would
  // their artificial clones. This would complicate the whole stuff a lot :)
  Iso_rectangle_2 crect = extend(boundingBox(sites), 0.1*SF);
  std::cout << "rect: " << crect << std::endl;

  // add artificial sites
  for (itr = sites.begin(); itr != sites.end(); ++itr) {
    Site_2 site = *itr;
    Point_2 point = site.point();
    double weight = site.weight();
    Point_2 apoint1(2 * crect.xmin() - point.x(), point.y());
    Site_2 asite1(apoint1, weight);
    Point_2 apoint2(2 * crect.xmax() - point.x(), point.y());
    Site_2 asite2(apoint2, weight);
    Point_2 apoint3(point.x(), 2 * crect.ymin() - point.y());
    Site_2 asite3(apoint3, weight);
    Point_2 apoint4(point.x(), 2 * crect.ymax() - point.y());
    Site_2 asite4(apoint4, weight);
    artificialSites.push_back(asite1);
    artificialSites.push_back(asite2);
    artificialSites.push_back(asite3);
    artificialSites.push_back(asite4);
  }

  // add all original sites to the apollonius graph
  for (itr = sites.begin(); itr != sites.end(); ++itr) {
    Site_2 site = *itr;
    ag.insert(site);
  }
  // add all artificial sites to the apollonius graph
  for (itr = artificialSites.begin(); itr != artificialSites.end(); ++itr) {
    Site_2 site = *itr;
    ag.insert(site);
  }

  // validate the Apollonius graph
  assert( ag.is_valid(true, 1) );
  std::cout << std::endl;

  // print the number of faces created
  size_t nof = ag.number_of_faces ();
  std::cout << "number of faces: " << nof << std::endl;

  // create the voronoi diagram
  Voronoi_diagram vd(ag);
  for (VoronoiFace_iterator fiter = vd.faces_begin(); fiter != vd.faces_end(); ++fiter) {
    std::cout << "vd face, bounded? " << !fiter->is_unbounded() << std::endl;
  }
  std::cout << "done iterating faces" << std::endl;

  /*
   * visualize with Qt
   */

  QApplication app(argc, argv);
  // Prepare scene
  QGraphicsScene scene;
  QRectF rect(crect.xmin(), crect.ymin(), 
      crect.xmax() - crect.xmin(), crect.ymax() - crect.ymin());
  scene.setSceneRect(rect);

  double pw = 0.01 * SF;
  double ew = 0.01 * SF;
  double lw = 0.005 * SF;

  // add a circle for each site
  std::cout << "creating circles for sites" << std::endl;
  for (Finite_vertices_iterator viter = ag.finite_vertices_begin();
    viter != ag.finite_vertices_end(); ++viter) {
    Site_2 site = viter->site();
    Point_2 p = site.point();
    double weight = site.weight();
    double size = weight * 2;

    scene.addEllipse(
      p.x() - weight, p.y() - weight, size, size,
      QPen(Qt::red, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
    );
  }

  // add dual image stubs
  std::cout << "dual stubs 1" << std::endl;
  for (All_faces_iterator fiter = ag.all_faces_begin (); fiter != ag.all_faces_end(); ++fiter) { 
    Object_2 o = ag.dual(fiter);
    Site_2 site;
    Line_2 line;
    if (assign(site, o)) {
      Point_2 p = site.point();
      double size = ew;
      scene.addEllipse(
         p.x() - size/2, p.y() - size/2, size, size,
         QPen(Qt::blue, pw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
       );
    } else if (assign(line, o)) {
    }
  }

  // add dual image stubs
  std::cout << "dual stubs 2" << std::endl;
  for (All_faces_iterator fiter = ag.all_faces_begin (); fiter != ag.all_faces_end(); ++fiter) { 
    Object_2 o = ag.dual(fiter);
    Site_2 site;
    Line_2 line;
    if (assign(site, o)) {
      Point_2 p = site.point();
      double size = ew;
      scene.addEllipse(
         p.x() - size/2, p.y() - size/2, size, size,
         QPen(Qt::blue, pw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)
       );
    } else if (assign(line, o)) {
    }
  }
  std::cout << "number of vertices: " << ag.number_of_vertices() << std::endl;
  std::cout << "vertices iteration 1" << std::endl;
  for (All_vertices_iterator viter = ag.all_vertices_begin (); viter != ag.all_vertices_end(); ++viter) { 
    Face_circulator fcirc = ag.incident_faces(viter), done(fcirc);
    if (fcirc == NULL) {
      break;
    }
    do {
      Object_2 o = ag.dual(fcirc);
    } while(++fcirc != done);
  }

  // draw the bounding box on the scene
  scene.addLine(
    QLineF(crect.xmin(), crect.ymin(), crect.xmax(), crect.ymin()),
    QPen(Qt::black, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  scene.addLine(
    QLineF(crect.xmax(), crect.ymin(), crect.xmax(), crect.ymax()),
    QPen(Qt::black, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  scene.addLine(
    QLineF(crect.xmax(), crect.ymax(), crect.xmin(), crect.ymax()),
    QPen(Qt::black, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  scene.addLine(
    QLineF(crect.xmin(), crect.ymax(), crect.xmin(), crect.ymin()),
    QPen(Qt::black, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

  // we want an identifier for each vertex within the iteration.
  int vertexIndex = 0;

  // for each vertex in the apollonius graph (this are the sites)
  for (All_vertices_iterator viter = ag.all_vertices_begin (); 
      viter != ag.all_vertices_end(); ++viter) { 
    // get the corresponding site
    Site_2 site = viter->site();
    Point_2 point = site.point();
    // ignore artifical sites, detect them by their position
    if (!containsPoint(crect, point)) {
      continue;
    }
    std::cout << "vertex " << vertexIndex << std::endl;

    // we than circulate all incident edges. By obtaining the respective
    // dual of each edge, we get access to the objects forming the boundary
    // of each voronoi cell in a proper order.
    Edge_circulator ecirc = ag.incident_edges(viter), done(ecirc);
    // this is where we store the polylines
    std::vector<std::vector<Point_2> > polylines;
    // for each incident edge
    do {
      // the program may fail in certain situations without this test.
      // acutally !is_infinite(edge) is a precondition in ag.dual(edge).
      if (ag.is_infinite(*ecirc)) {
        continue;
      }
      // NOTE: for this to work, we had to make public the dual function in ApolloniusGraph
      // change line 542 in "Apollonius_graph_2.h" from "private:" to "public:"
      Object_2 o = ag.dual(*ecirc);
      Line_2 l;
      Segment_2 s;
      Ray_2 r;
      CGAL::Hyperbola_2<Traits> h;
      CGAL::Hyperbola_segment_2<Traits> hs;
      CGAL::Hyperbola_ray_2<Traits> hr;
      if (assign(hs, o)) {
        std::cout << "hyperbola segment" << std::endl; //hs.draw(str);
        std::vector<Point_2> p;
        hs.generate_points(p);
        std::cout << "# hyperbola points: " << p.size() << std::endl;
        std::vector<Point_2> points;
        points.insert(points.end(), p.begin(), p.end());
        polylines.push_back(points);
        for (unsigned int i = 0; i < p.size() - 1; i++) {
          Segment_2 seg(p[i], p[i+1]);
          scene.addLine(
            QLineF(p[i].x(), p[i].y(), p[i+1].x(), p[i+1].y()),
            QPen(Qt::green, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
      }
      else if (assign(s, o)) {
        Point_2 ss = s.source();
        Point_2 st = s.target();
        std::cout << "segment " << ss << " " << st << std::endl; // str << s; 
        std::vector<Point_2> points;
        points.push_back(ss);
        points.push_back(st);
        polylines.push_back(points);
        scene.addLine(
          QLineF(ss.x(), ss.y(), st.x(), st.y()),
          QPen(Qt::green, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      }
      else if (assign(hr, o)) {
        std::cout << "hyperbola ray" << std::endl; // hr.draw(str);
        std::vector<Point_2> p;
        hr.generate_points(p);
        std::vector<Point_2> points;
        points.insert(points.end(), p.begin(), p.end());
        polylines.push_back(points);
        for (unsigned int i = 0; i < p.size() - 1; i++) {
          Segment_2 seg(p[i], p[i+1]);
          scene.addLine(
            QLineF(p[i].x(), p[i].y(), p[i+1].x(), p[i+1].y()),
            QPen(Qt::green, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
      }
      else if (assign(r, o)) {
        std::cout << "ray" << std::endl;  // str << r;
        Object_2 o = CGAL::intersection(crect, r);

        Segment_2 seg;
        Point_2 pnt;
        if (assign(seg, o)) {
          std::cout << "ray -> segment" << std::endl;
          Point_2 ss = seg.source();
          Point_2 st = seg.target();
          std::vector<Point_2> points;
          points.push_back(ss);
          points.push_back(st);
          polylines.push_back(points);
          scene.addLine(
            QLineF(ss.x(), ss.y(), st.x(), st.y()),
            QPen(Qt::green, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        } else if (assign(pnt, o)){
          std::cout << "ray -> point" << std::endl;
          // no use for points
        } else {
          std::cout << "ray -> ?" << std::endl;
          std::cout << r.source() << " " << r.point(1) << std::endl;
        }
      }
      else if (assign(h, o)) {
        std::cout << "hyperbola" << std::endl; // h.draw(str);
        std::vector<Point_2> p, q;
        h.generate_points(p, q);
        // TODO: we are not adding anything to points here. This is kind of
        // obsolete since this cannot happen anymore (because of the 
        // artificial sites added in the beginning)
        for (unsigned int i = 0; i < p.size() - 1; i++) {
          Segment_2 seg(p[i], p[i+1]);
          scene.addLine(
            QLineF(p[i].x(), p[i].y(), p[i+1].x(), p[i+1].y()),
            QPen(Qt::green, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
        for (unsigned int i = 0; i < q.size() - 1; i++) {
          Segment_2 seg(q[i], q[i+1]);
          scene.addLine(
            QLineF(q[i].x(), q[i].y(), q[i+1].x(), q[i+1].y()),
            QPen(Qt::green, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
      }
      else if (assign(l, o)) {
        std::cout << "line" << std::endl; //str << l;
        Object_2 o = CGAL::intersection(l, crect);

        Segment_2 seg;
        Point_2 pnt;
        if (assign(seg, o)) {
          std::cout << "line -> segment" << std::endl;
          Point_2 ss = seg.source();
          Point_2 st = seg.target();
          std::vector<Point_2> points;
          points.push_back(ss);
          points.push_back(st);
          polylines.push_back(points);
          scene.addLine(
            QLineF(ss.x(), ss.y(), st.x(), st.y()),
            QPen(Qt::green, lw,  Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        } else if (assign(pnt, o)){
          std::cout << "line -> point" << std::endl;
          // no use for points
        }
      }
    } while(++ecirc != done);

    // A list for the resulting points.
    std::vector<Point_2> points;
    // We build this list from the polylines collected before.
    // One of the problems solved here is that we actually don't know 
    // whether the polylines we get are in order or reversed because of the
    // way we obtained them via Hyperbola_2.generate_points which makes no
    // guarantees about the order of produces points. So we check at each
    // step whether a consecutive polyline has to be added in-order or
    // reversed by inspecting the first and last points respectively. A
    // special case is the first polyline, where we don't have a previous
    // polyline to perform this check for, which is why we make a more 
    // extensive checking when adding the second polyline which also takes
    // care about the first.
    std::cout << "# polylines: " << polylines.size() << std::endl;
    // the first polyline will just be added as it is
    if (polylines.size() > 0) {
      std::vector<Point_2> seg = polylines.front();
      points.insert(points.end(), seg.begin(), seg.end());
    }
    // handle consecutive polylines
    if (polylines.size() > 1) {
      // check wheter we can use the second segment this way or whether we
      // have to reverse the first polyline. Use distance comparism to pick
      // the best fit (so that we don't need exact equivalence of endpoints)
      std::vector<Point_2> seg = polylines.at(1);
      double d1 = CGAL::squared_distance(points.front(), seg.front());
      double d2 = CGAL::squared_distance(points.front(), seg.back());
      double d3 = CGAL::squared_distance(points.back(), seg.front());
      double d4 = CGAL::squared_distance(points.back(), seg.back());
      // if the first point of the first polyline fits both endpoints of the
      // second polyline better thant the last point of the first polyline,
      // then we reverse the order of the first polyline.
      if ((d1 < d3 && d1 < d4) || (d2 < d3 && d2 < d4)) {
        std::reverse(points.begin(), points.end());
      }
      // for each consecutive polyline
      for (int i = 1; i < polylines.size(); i++) {
        // check which endpoint of this polyline is nearest to the last
        // point in our list of points.
        Point_2 lastPoint = points.back();
        std::vector<Point_2> seg = polylines.at(i);
        double d1 = CGAL::squared_distance(lastPoint, seg.front());
        double d2 = CGAL::squared_distance(lastPoint, seg.back());
        if (d1 <= d2) {
          // first point fits better, take polyline in default order
          points.insert(points.end(), ++seg.begin(), seg.end());
        } else {
          // last point fits better, take polyline in reverse order
          points.insert(points.end(), ++seg.rbegin(), seg.rend());
        }
      }
    }
    std::cout << "# points: " << points.size() << std::endl;

    // close polygon if necessary
    if (points.size() > 0){
      Point_2 start = points.front();
      Point_2 end = points.back();
      if (start != end) {
        points.push_back(start);
      }
    }

    if (points.size() > 0 && points.size() < 4) {
      std::cout << "invalid polygon: >0 but <4 points" << std::endl;
      points.clear();
    }

    // open an output file for storing the WKT
    vertexIndex++;
    std::stringstream s;
    s << outdir << "/" << vertexIndex << ".wkt";
    std::string polygonFileName = s.str();
    std::cout << "filename: " << polygonFileName << std::endl;
    std::ofstream wktFile;
    wktFile.open(polygonFileName.c_str());

    // write WKT file
    wktFile << "POLYGON ((";
    for (int i = 0; i < points.size(); i++) {
      Point_2 p = points.at(i);
      wktFile << p.x()/SF << " " << p.y()/SF;
      if (i < points.size() - 1) {
        wktFile << ", ";
      }
    }
    wktFile << "))";
    wktFile.close();

    // check each point
    for (int i = 0; i < points.size(); i++) {
      Point_2 p = points.at(i);
      if (p.x() > crect.xmax() || p.x() < crect.xmin() || p.y() > crect.ymax() || p.y() < crect.ymin()) {
        std::cout << "out of bounds" << std::endl;
      }
    }
  }

  // Prepare view, add scene, show
  QGraphicsView* view = new QGraphicsView(&scene);
  //view->show();

  // Add CGAL's navigation filter
  CGAL::Qt::GraphicsViewNavigation navigation;
  view->installEventFilter(&navigation);
  view->viewport()->installEventFilter(&navigation);
  view->setRenderHint(QPainter::Antialiasing);

  // create an image
  // this line creates an image with a size definied by the size of the scene
  //QImage image(scene.sceneRect().size().toSize(), QImage::Format_ARGB32);
  // Cool! Qt does the scaling to arbitrary size automatically. Rocks
  QImage image(QSize(2000, 2000), QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing);
  scene.render(&painter);
  image.save(output);
 
  //return app.exec();  
}

/*
 * Test whether the point lies within the specified rectangle
 */
bool containsPoint(Iso_rectangle_2 rect, Point_2 point) {
  return point.x() >= rect.xmin() && point.x() <= rect.xmax()
      && point.y() >= rect.ymin() && point.y() <= rect.ymax();
}

/*
 * calculate the bounding box of all specified sites
 */
Iso_rectangle_2 boundingBox(std::vector<Site_2> sites) {
  double xmin, xmax, ymin, ymax;
  xmin = xmax = sites.front().point().x();
  ymin = ymax = sites.front().point().y();
  std::vector<Site_2>::iterator itr;
  for (itr = sites.begin(); itr != sites.end(); ++itr) {
    Site_2 site = *itr;
    Point_2 point = site.point();
    if (point.x() < xmin) {
      xmin = point.x();
    }
    if (point.x() > xmax) {
      xmax = point.x();
    }
    if (point.y() < ymin) {
      ymin = point.y();
    }
    if (point.y() > ymax) {
      ymax = point.y();
    }
  }
  Iso_rectangle_2 rect(xmin, ymin, xmax, ymax);
  return rect;
}

/*
 * create a rectangle based on the specifed one, but enlarge it by amount
 */
Iso_rectangle_2 extend(Iso_rectangle_2 rect, double amount) {
  Iso_rectangle_2 r(rect.xmin()-amount, rect.ymin()-amount, 
      rect.xmax()+amount, rect.ymax()+amount);
  return r;
}

