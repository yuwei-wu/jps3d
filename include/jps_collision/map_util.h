/**
 * @file map_util.h
 * @brief MapUtil classes
 */
#ifndef JPS_MAP_UTIL_H
#define JPS_MAP_UTIL_H

#include <iostream>
#include <jps_basis/data_type.h>
#include <pcl/point_cloud.h>
#include <pcl/kdtree/kdtree_flann.h>

namespace JPS {
  ///The type of map data Tmap is defined as a 1D array
  using Tmap = std::vector<signed char>;
  /**
   * @biref The map util class for collision checking
   * @param Dim is the dimension of the workspace
   */
  template <int Dim> class MapUtil {
    public:
      ///Simple constructor
      MapUtil() {}

      /**
       * @brief Init map
       *
       * @param ori origin position
       * @param dim number of cells in each dimension
       * @param res map resolution
       */

      void initMap(const Veci<Dim> dim, Vecf<Dim> ori, decimal_t res, decimal_t inflated_ratio){
        map_.clear();
        for (unsigned int i = 0; i < Dim; i++){
          dim_(i) = dim(i);
          origin_d_(i) = ori[i];
        }
        res_ = res;
        inflated_r_ = inflated_ratio;
        total_size_ = Dim == 2 ? dim_(0) * dim_(1) :
                                 dim_(0) * dim_(1) * dim_(2);
        map_.resize(total_size_, 0);
        std::fill(map_.begin(), map_.end(), val_free);

      }

      /**
       * @brief update Cylinders
       *
       * @param cylinder_ptr new cylinders // only for 2-d map
       */
      void updateCylinders(pcl::PointCloud<pcl::PointXYZ>::Ptr cylinder_ptr){
        

        for (size_t i = 0; i < cylinder_ptr->points.size(); ++i)
        {
          int x = std::max((int)std::round((cylinder_ptr->points[i].x - origin_d_(0)) / res_ - 0.5), 0);
          int y = std::max((int)std::round((cylinder_ptr->points[i].y - origin_d_(1)) / res_ - 0.5), 0);
          double width = cylinder_ptr->points[i].z;
          
          // this next formula works only when x, y, z are in cell coordinates (relative to the origin of the map)
          int id = x + dim_(0) * y;

          if (id == val_cyl_crt){
            continue;
          }

          if (id >= 0 && id < total_size_)
          {
            map_[id] = val_cyl_crt;
          }         

          int inf_step = ceil((1.0 + inflated_r_)*width*0.5 / res_); 
          // fine, we can directly set it as the bounding box
          // @yuwei
          for (int ix = x - inf_step; ix <= x + inf_step; ix++)
          {
            for (int iy = y - inf_step; iy <= y + inf_step; iy++)
            { 
              int id_infl = ix + dim_(0) * iy;

              if (id_infl >= 0 && id_infl < total_size_ && id_infl != val_cyl_crt)  // Ensure we are inside the map
              {
                map_[id_infl] = val_occ;
              }
            }
          }
        }
      }


      void buildSSMap(pcl::PointCloud<pcl::PointXYZ>::Ptr cylinder_ptr){
        
      }


      // the point cloud has been inflated
      void readMap(pcl::PointCloud<pcl::PointXYZ>::Ptr pclptr)
      {
      
        for (size_t i = 0; i < pclptr->points.size(); ++i)
        {
          // find the cell coordinates of the point expresed in a system of coordinates that has as origin the (minX,
          // minY, minZ) point of the map
          int x = std::round((pclptr->points[i].x - origin_d_(0)) / res_ - 0.5);
          int y = std::round((pclptr->points[i].y - origin_d_(1)) / res_ - 0.5);
          int z = std::round((pclptr->points[i].z - origin_d_(2)) / res_ - 0.5);

          // Force them to be positive:
          x = (x > 0) ? x : 0;
          y = (y > 0) ? y : 0;
          z = (z > 0) ? z : 0;
          // this next formula works only when x, y, z are in cell coordinates (relative to the origin of the map)
          int id = x + dim_(0) * y + dim_(0) * dim_(1) * z;

          if (id >= 0 && id < total_size_)
          {
            map_[id] = val_occ;
          }
          // now let's inflate the voxels around that point
          int m = ceil(inflated_r_ / res_);
          // m is the amount of cells to inflate in each direction
          for (int ix = x - m; ix <= x + m; ix++)
          {
            for (int iy = y - m; iy <= y + m; iy++)
            {
              for (int iz = z - m; iz <= z + m; iz++)
              {
                int id_infl = ix + dim_(0) * iy + dim_(0) * dim_(1) * iz;
                if (id_infl >= 0 && id_infl < total_size_)  // Ensure we are inside the map
                {
                  map_[id_infl] = val_occ;
                }
              }
            }
          }
        } 
      }

      ///Get map data
      Tmap getMap() { return map_; }
      ///Get resolution
      decimal_t getRes() { return res_; }
      ///Get dimensions
      Veci<Dim> getDim() { return dim_; }
      ///Get origin
      Vecf<Dim> getOrigin() { return origin_d_; }
      ///Get index of a cell
      int getIndex(const Veci<Dim>& pn) {
          return Dim == 2 ? pn(0) + dim_(0) * pn(1) :
                            pn(0) + dim_(0) * pn(1) + dim_(0) * dim_(1) * pn(2);
      }

      ///Check if the given cell is outside of the map in i-the dimension
      bool isOutsideXYZ(const Veci<Dim> &n, int i) { return n(i) < 0 || n(i) >= dim_(i); }
      ///Check if the cell is free by index
      bool isFree(int idx) { return map_[idx] == val_free; }
      ///Check if the cell is unknown by index
      bool isUnknown(int idx) { return map_[idx] == val_unknown; }
      ///Check if the cell is occupied by index
      bool isOccupied(int idx) { return map_[idx] > val_free; }

      ///Check if the cell is outside by coordinate
      bool isOutside(const Veci<Dim> &pn) {
        for(int i = 0; i < Dim; i++)
          if (pn(i) < 0 || pn(i) >= dim_(i))
            return true;
        return false;
      }
      ///Check if the given cell is free by coordinate
      bool isFree(const Veci<Dim> &pn) {
        if (isOutside(pn))
          return false;
        else
          return isFree(getIndex(pn));
      }
      ///Check if the given cell is occupied by coordinate
      bool isOccupied(const Veci<Dim> &pn) {
        if (isOutside(pn))
          return false;
        else
          return isOccupied(getIndex(pn));
      }
      ///Check if the given cell is unknown by coordinate
      bool isUnknown(const Veci<Dim> &pn) {
        if (isOutside(pn))
          return false;
        return map_[getIndex(pn)] == val_unknown;
      }

      /**
       * @brief Set map
       *
       * @param ori origin position
       * @param dim number of cells in each dimension
       * @param map array of cell values
       * @param res map resolution
       */
      void setMap(const Vecf<Dim>& ori, const Veci<Dim>& dim, const Tmap &map, decimal_t res) {
        map_ = map;
        dim_ = dim;
        origin_d_ = ori;
        res_ = res;
      }

      ///Print basic information about the util
      void info() {
        Vecf<Dim> range = dim_.template cast<decimal_t>() * res_;
        std::cout << "MapUtil Info ========================== " << std::endl;
        std::cout << "   res: [" << res_ << "]" << std::endl;
        std::cout << "   origin: [" << origin_d_.transpose() << "]" << std::endl;
        std::cout << "   range: [" << range.transpose() << "]" << std::endl;
        std::cout << "   dim: [" << dim_.transpose() << "]" << std::endl;
      };

      ///Float position to discrete cell coordinate
      Veci<Dim> floatToInt(const Vecf<Dim> &pt) {
        Veci<Dim> pn;
        for(int i = 0; i < Dim; i++)
          pn(i) = std::round((pt(i) - origin_d_(i)) / res_ - 0.5);
        return pn;
      }
      ///Discrete cell coordinate to float position
      Vecf<Dim> intToFloat(const Veci<Dim> &pn) {
        //return pn.template cast<decimal_t>() * res_ + origin_d_;
        return (pn.template cast<decimal_t>() + Vecf<Dim>::Constant(0.5)) * res_ + origin_d_;
      }

      ///Raytrace from float point pt1 to pt2
      vec_Veci<Dim> rayTrace(const Vecf<Dim> &pt1, const Vecf<Dim> &pt2) {
        Vecf<Dim> diff = pt2 - pt1;
        decimal_t k = 0.8;
        int max_diff = (diff / res_).template lpNorm<Eigen::Infinity>() / k;
        decimal_t s = 1.0 / max_diff;
        Vecf<Dim> step = diff * s;

        vec_Veci<Dim> pns;
        Veci<Dim> prev_pn = Veci<Dim>::Constant(-1);
        for (int n = 1; n < max_diff; n++) {
          Vecf<Dim> pt = pt1 + step * n;
          Veci<Dim> new_pn = floatToInt(pt);
          if (isOutside(new_pn))
            break;
          if (new_pn != prev_pn)
            pns.push_back(new_pn);
          prev_pn = new_pn;
        }
        return pns;
      }

      ///Check if the ray from p1 to p2 is occluded
      bool isBlocked(const Vecf<Dim>& p1, const Vecf<Dim>& p2, int8_t val = 100) {
        vec_Veci<Dim> pns = rayTrace(p1, p2);
        for (const auto &pn : pns) {
          if (map_[getIndex(pn)] >= val)
            return true;
        }
        return false;
      }

      ///Get occupied voxels
      vec_Vecf<Dim> getCloud() {
        vec_Vecf<Dim> cloud;
        Veci<Dim> n;
        if(Dim == 3) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              for (n(2) = 0; n(2) < dim_(2); n(2)++) {
                if (isOccupied(getIndex(n)))
                  cloud.push_back(intToFloat(n));
              }
            }
          }
        }
        else if (Dim == 2) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              if (isOccupied(getIndex(n)))
                cloud.push_back(intToFloat(n));
            }
          }
        }

        return cloud;
      }


      void getpclCloud(pcl::PointCloud<pcl::PointXYZ>::Ptr &pclptr) {
        Veci<Dim> n;
        if(Dim == 3) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              for (n(2) = 0; n(2) < dim_(2); n(2)++) {
                if (isOccupied(getIndex(n))){
                  pcl::PointXYZ pt;
                  auto temp =  intToFloat(n);
                  pt.x = temp(0);
                  pt.y = temp(1);
                  pt.z = temp(2);
                  pclptr->points.push_back(pt);
                }
              }
            }
          }
        }
        else if (Dim == 2) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              if (isOccupied(getIndex(n))){
                pcl::PointXYZ pt;
                auto  temp =  intToFloat(n);
                pt.x = temp(0);
                pt.y = temp(1);
                pt.z = 0.2;
                pclptr->points.push_back(pt);
              }
            }
          }
        }
      }


      ///Get free voxels
      vec_Vecf<Dim> getFreeCloud() {
        vec_Vecf<Dim> cloud;
        Veci<Dim> n;
        if(Dim == 3) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              for (n(2) = 0; n(2) < dim_(2); n(2)++) {
                if (isFree(getIndex(n)))
                  cloud.push_back(intToFloat(n));
              }
            }
          }
        }
        else if (Dim == 2) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              if (isFree(getIndex(n)))
                cloud.push_back(intToFloat(n));
            }
          }
        }

        return cloud;
      }

      ///Get unknown voxels
      vec_Vecf<Dim> getUnknownCloud() {
        vec_Vecf<Dim> cloud;
        Veci<Dim> n;
        if(Dim == 3) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              for (n(2) = 0; n(2) < dim_(2); n(2)++) {
                if (isUnknown(getIndex(n)))
                  cloud.push_back(intToFloat(n));
              }
            }
          }
        }
        else if (Dim == 2) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              if (isUnknown(getIndex(n)))
                cloud.push_back(intToFloat(n));
            }
          }
        }

        return cloud;
      }

      ///Dilate occupied cells
      void dilate(const vec_Veci<Dim>& dilate_neighbor) {
        Tmap map = map_;
        Veci<Dim> n = Veci<Dim>::Zero();
        if(Dim == 3) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              for (n(2) = 0; n(2) < dim_(2); n(2)++) {
                if (isOccupied(getIndex(n))) {
                  for (const auto &it : dilate_neighbor) {
                    if (!isOutside(n + it))
                      map[getIndex(n + it)] = val_occ;
                  }
                }
              }
            }
          }
        }
        else if(Dim == 2) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              if (isOccupied(getIndex(n))) {
                for (const auto &it : dilate_neighbor) {
                  if (!isOutside(n + it))
                    map[getIndex(n + it)] = val_occ;
                }
              }
            }
          }
        }

        map_ = map;
      }

      ///Free unknown voxels
      void freeUnknown() {
        Veci<Dim> n;
        if(Dim == 3) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              for (n(2) = 0; n(2) < dim_(2); n(2)++) {
                if (isUnknown(getIndex(n)))
                  map_[getIndex(n)] = val_free;
              }
            }
          }
        }
        else if (Dim == 2) {
          for (n(0) = 0; n(0) < dim_(0); n(0)++) {
            for (n(1) = 0; n(1) < dim_(1); n(1)++) {
              if (isUnknown(getIndex(n)))
                map_[getIndex(n)] = val_free;
            }
          }
        }
      }

      ///Map entity
      Tmap map_;
    protected:
      ///Resolution
      decimal_t res_;
      // Inflated ratio
      decimal_t inflated_r_;
      // total size
      double total_size_;
      ///Origin, float type
      Vecf<Dim> origin_d_;
      ///Dimension, int type
      Veci<Dim> dim_;
      ///Assume occupied cell has value 100
      int8_t val_occ = 100;
      ///Assume free cell has value 0
      int8_t val_free = 0;
      ///Assume unknown cell has value -1
      int8_t val_unknown = -1;
      ///Center of Cylinders
      int8_t val_cyl_crt = 50;
  };

  typedef MapUtil<2> OccMapUtil;

  typedef MapUtil<3> VoxelMapUtil;

}

#endif
