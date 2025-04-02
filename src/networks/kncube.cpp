// $Id$

/*
 Copyright (c) 2007-2015, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*kn.cpp
 *
 *Meshs, cube, torus
 *
 */

#include "booksim.hpp"
#include <vector>
#include <sstream>
#include <ctime>
#include <cassert>
#include "kncube.hpp"
#include "random_utils.hpp"
#include "misc_utils.hpp"
// #include "iq_router.hpp"

float gThresholdMultiplier = 0.5;

KNCube::KNCube(const Configuration &config, const string &name, bool mesh) : Network(config, name)
{
  _mesh = mesh;

  _ComputeSize(config);
  _Alloc();
  _BuildNet(config);

  gThresholdMultiplier = config.GetFloat("threshold_multiplier");
}

void KNCube::_ComputeSize(const Configuration &config)
{
  _k = config.GetInt("k");
  _n = config.GetInt("n");

  gK = _k;
  gN = _n;
  _size = powi(_k, _n);
  _channels = 2 * _n * _size;

  _nodes = _size;
}

void KNCube::RegisterRoutingFunctions()
{
  gRoutingFunctionMap["custom_turn_local_mesh"] = &custom_route;
  gRoutingFunctionMap["custom__turn_global_mesh"] = &custom_global_route;
  gRoutingFunctionMap["odd_even_mesh"] = &odd_even_route;
  gRoutingFunctionMap["custom_min_adaptive_mesh"] = &custom_min_adaptive_route;
  gRoutingFunctionMap["custom_min_adaptive_local_mesh"] = &custom_min_adaptive_local_route;
  gRoutingFunctionMap["custom_min_adaptive_global_mesh"] = &custom_min_adaptive_global_route;
}

void KNCube::_BuildNet(const Configuration &config)
{
  int left_node;
  int right_node;

  int right_input;
  int left_input;

  int right_output;
  int left_output;

  ostringstream router_name;

  // latency type, noc or conventional network
  bool use_noc_latency;
  use_noc_latency = (config.GetInt("use_noc_latency") == 1);

  for (int node = 0; node < _size; ++node)
  {

    router_name << "router";

    if (_k > 1)
    {
      for (int dim_offset = _size / _k; dim_offset >= 1; dim_offset /= _k)
      {
        router_name << "_" << (node / dim_offset) % _k;
      }
    }

    _routers[node] = Router::NewRouter(config, this, router_name.str(),
                                       node, 2 * _n + 1, 2 * _n + 1);
    _timed_modules.push_back(_routers[node]);

    router_name.str("");

    for (int dim = 0; dim < _n; ++dim)
    {

      // find the neighbor
      left_node = _LeftNode(node, dim);
      right_node = _RightNode(node, dim);

      //
      // Current (N)ode
      // (L)eft node
      // (R)ight node
      //
      //   L--->N<---R
      //   L<---N--->R
      //

      // torus channel is longer due to wrap around
      int latency = _mesh ? 1 : 2;

      // get the input channel number
      right_input = _LeftChannel(right_node, dim);
      left_input = _RightChannel(left_node, dim);

      // add the input channel
      _routers[node]->AddInputChannel(_chan[right_input], _chan_cred[right_input]);
      _routers[node]->AddInputChannel(_chan[left_input], _chan_cred[left_input]);

      // set input channel latency
      if (use_noc_latency)
      {
        _chan[right_input]->SetLatency(latency);
        _chan[left_input]->SetLatency(latency);
        _chan_cred[right_input]->SetLatency(latency);
        _chan_cred[left_input]->SetLatency(latency);
      }
      else
      {
        _chan[left_input]->SetLatency(1);
        _chan_cred[right_input]->SetLatency(1);
        _chan_cred[left_input]->SetLatency(1);
        _chan[right_input]->SetLatency(1);
      }
      // get the output channel number
      right_output = _RightChannel(node, dim);
      left_output = _LeftChannel(node, dim);

      // add the output channel
      _routers[node]->AddOutputChannel(_chan[right_output], _chan_cred[right_output]);
      _routers[node]->AddOutputChannel(_chan[left_output], _chan_cred[left_output]);

      // set output channel latency
      if (use_noc_latency)
      {
        _chan[right_output]->SetLatency(latency);
        _chan[left_output]->SetLatency(latency);
        _chan_cred[right_output]->SetLatency(latency);
        _chan_cred[left_output]->SetLatency(latency);
      }
      else
      {
        _chan[right_output]->SetLatency(1);
        _chan[left_output]->SetLatency(1);
        _chan_cred[right_output]->SetLatency(1);
        _chan_cred[left_output]->SetLatency(1);
      }
    }
    // injection and ejection channel, always 1 latency
    _routers[node]->AddInputChannel(_inject[node], _inject_cred[node]);
    _routers[node]->AddOutputChannel(_eject[node], _eject_cred[node]);
    _inject[node]->SetLatency(1);
    _eject[node]->SetLatency(1);
  }
}

int KNCube::_LeftChannel(int node, int dim)
{
  // The base channel for a node is 2*_n*node
  int base = 2 * _n * node;
  // The offset for a left channel is 2*dim + 1
  int off = 2 * dim + 1;

  return (base + off);
}

int KNCube::_RightChannel(int node, int dim)
{
  // The base channel for a node is 2*_n*node
  int base = 2 * _n * node;
  // The offset for a right channel is 2*dim
  int off = 2 * dim;
  return (base + off);
}

int KNCube::_LeftNode(int node, int dim)
{
  int k_to_dim = powi(_k, dim);
  int loc_in_dim = (node / k_to_dim) % _k;
  int left_node;
  // if at the left edge of the dimension, wraparound
  if (loc_in_dim == 0)
  {
    left_node = node + (_k - 1) * k_to_dim;
  }
  else
  {
    left_node = node - k_to_dim;
  }

  return left_node;
}

int KNCube::_RightNode(int node, int dim)
{
  int k_to_dim = powi(_k, dim);
  int loc_in_dim = (node / k_to_dim) % _k;
  int right_node;
  // if at the right edge of the dimension, wraparound
  if (loc_in_dim == (_k - 1))
  {
    right_node = node - (_k - 1) * k_to_dim;
  }
  else
  {
    right_node = node + k_to_dim;
  }

  return right_node;
}

int KNCube::GetN() const
{
  return _n;
}

int KNCube::GetK() const
{
  return _k;
}

/*legacy, not sure how this fits into the new scheme of things*/
void KNCube::InsertRandomFaults(const Configuration &config)
{
  int num_fails = config.GetInt("link_failures");

  if (_size && num_fails)
  {
    vector<long> save_x;
    vector<double> save_u;
    SaveRandomState(save_x, save_u);
    int fail_seed;
    if (config.GetStr("fail_seed") == "time")
    {
      fail_seed = int(time(NULL));
      cout << "SEED: fail_seed=" << fail_seed << endl;
    }
    else
    {
      fail_seed = config.GetInt("fail_seed");
    }
    RandomSeed(fail_seed);

    vector<bool> fail_nodes(_size);

    for (int i = 0; i < _size; ++i)
    {
      int node = i;

      // edge test
      bool edge = false;
      for (int n = 0; n < _n; ++n)
      {
        if (((node % _k) == 0) ||
            ((node % _k) == _k - 1))
        {
          edge = true;
        }
        node /= _k;
      }

      if (edge)
      {
        fail_nodes[i] = true;
      }
      else
      {
        fail_nodes[i] = false;
      }
    }

    for (int i = 0; i < num_fails; ++i)
    {
      int j = RandomInt(_size - 1);
      bool available = false;
      int node = -1;
      int chan = -1;
      int t;

      for (t = 0; (t < _size) && (!available); ++t)
      {
        int node = (j + t) % _size;

        if (!fail_nodes[node])
        {
          // check neighbors
          int c = RandomInt(2 * _n - 1);

          for (int n = 0; (n < 2 * _n) && (!available); ++n)
          {
            chan = (n + c) % 2 * _n;

            if (chan % 1)
            {
              available = fail_nodes[_LeftNode(node, chan / 2)];
            }
            else
            {
              available = fail_nodes[_RightNode(node, chan / 2)];
            }
          }
        }

        if (!available)
        {
          cout << "skipping " << node << endl;
        }
      }

      if (t == _size)
      {
        Error("Could not find another possible fault channel");
      }

      assert(node != -1);
      assert(chan != -1);
      OutChannelFault(node, chan);
      fail_nodes[node] = true;

      for (int n = 0; (n < _n) && available; ++n)
      {
        fail_nodes[_LeftNode(node, n)] = true;
        fail_nodes[_RightNode(node, n)] = true;
      }

      cout << "failure at node " << node << ", channel "
           << chan << endl;
    }

    RestoreRandomState(save_x, save_u);
  }
}

double KNCube::Capacity() const
{
  return (double)_k / (_mesh ? 8.0 : 4.0);
}

int dor_next_mesh_2(int cur, int dest, bool descending)
{
  if (cur == dest)
  {
    return 2 * gN; // Eject
  }

  int dim_left;

  if (descending)
  {
    for (dim_left = (gN - 1); dim_left > 0; --dim_left)
    {
      if ((cur * gK / gNodes) != (dest * gK / gNodes))
      {
        break;
      }
      cur = (cur * gK) % gNodes;
      dest = (dest * gK) % gNodes;
    }
    cur = (cur * gK) / gNodes;
    dest = (dest * gK) / gNodes;
  }
  else
  {
    for (dim_left = 0; dim_left < (gN - 1); ++dim_left)
    {
      if ((cur % gK) != (dest % gK))
      {
        break;
      }
      cur /= gK;
      dest /= gK;
    }
    cur %= gK;
    dest %= gK;
  }

  if (cur < dest)
  {
    return 2 * dim_left; // Right
  }
  else
  {
    return 2 * dim_left + 1; // Left
  }
}

void custom_min_adaptive_route(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject)
{
  int vcBegin = 0, vcEnd = gNumVCs - 1;
  if (f->type == Flit::READ_REQUEST)
  {
    vcBegin = gReadReqBeginVC;
    vcEnd = gReadReqEndVC;
  }
  else if (f->type == Flit::WRITE_REQUEST)
  {
    vcBegin = gWriteReqBeginVC;
    vcEnd = gWriteReqEndVC;
  }
  else if (f->type == Flit::READ_REPLY)
  {
    vcBegin = gReadReplyBeginVC;
    vcEnd = gReadReplyEndVC;
  }
  else if (f->type == Flit::WRITE_REPLY)
  {
    vcBegin = gWriteReplyBeginVC;
    vcEnd = gWriteReplyEndVC;
  }
  assert(((f->vc >= vcBegin) && (f->vc <= vcEnd)) || (inject && (f->vc < 0)));

  outputs->Clear();

  if (inject)
  {
    // injection can use all VCs
    outputs->AddRange(-1, vcBegin, vcEnd);
    return;
  }
  else if (r->GetID() == f->dest)
  {
    // ejection can also use all VCs
    outputs->AddRange(2 * gN, vcBegin, vcEnd);
    return;
  }

  int in_vc;

  if (in_channel == 2 * gN)
  {
    in_vc = vcEnd; // ignore the injection VC
  }
  else
  {
    in_vc = f->vc;
  }

  // DOR for the escape channel (VC 0), low priority
  int out_port = dor_next_mesh_2(r->GetID(), f->dest, false);
  outputs->AddRange(out_port, 0, vcBegin, vcBegin);

  if (f->watch)
  {
    *gWatchOut << GetSimTime() << " | " << r->FullName() << " | "
               << "Adding VC range ["
               << vcBegin << ","
               << vcBegin << "]"
               << " at output port " << out_port
               << " for flit " << f->id
               << " (input port " << in_channel
               << ", destination " << f->dest << ")"
               << "." << endl;
  }

  if (in_vc != vcBegin)
  { // If not in the escape VC
    // Minimal adaptive for all other channels
    int cur = r->GetID();
    int dest = f->dest;

    for (int n = 0; n < gN; ++n)
    {
      if ((cur % gK) != (dest % gK))
      {
        // Add minimal direction in dimension 'n'
        if ((cur % gK) < (dest % gK))
        { // Right
          if (f->watch)
          {
            *gWatchOut << GetSimTime() << " | " << r->FullName() << " | "
                       << "Adding VC range ["
                       << (vcBegin + 1) << ","
                       << vcEnd << "]"
                       << " at output port " << 2 * n
                       << " with priority " << 1
                       << " for flit " << f->id
                       << " (input port " << in_channel
                       << ", destination " << f->dest << ")"
                       << "." << endl;
          }
          outputs->AddRange(2 * n, vcBegin + 1, vcEnd, 1);
        }
        else
        { // Left
          if (f->watch)
          {
            *gWatchOut << GetSimTime() << " | " << r->FullName() << " | "
                       << "Adding VC range ["
                       << (vcBegin + 1) << ","
                       << vcEnd << "]"
                       << " at output port " << 2 * n + 1
                       << " with priority " << 1
                       << " for flit " << f->id
                       << " (input port " << in_channel
                       << ", destination " << f->dest << ")"
                       << "." << endl;
          }
          outputs->AddRange(2 * n + 1, vcBegin + 1, vcEnd, 1);
        }
      }
      cur /= gK;
      dest /= gK;
    }
  }
}

void custom_min_adaptive_global_route(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject)
{
  if (inject)
  {
    outputs->Clear();
    outputs->AddRange(-1, 0, gNumVCs - 1);
    return;
  }

  int global_max_threshold = 0;
  for (int i = 0; i < 2 * gN; i++) // Check all input/output ports
  {
    global_max_threshold += r->GetBufferOccupancy(i);
  }
  int global_congestion_threshold = global_max_threshold * gThresholdMultiplier / gN; // Average across all ports

  int east_cong = r->GetUsedCredit(0);  // East port
  int west_cong = r->GetUsedCredit(1);  // West port
  int south_cong = r->GetUsedCredit(2); // South port
  int north_cong = r->GetUsedCredit(3); // North port

  int global_avg_congestion = (east_cong + west_cong + south_cong + north_cong) / 4;

  if (global_avg_congestion < global_congestion_threshold)
  {
    dim_order_route(r, f, in_channel, outputs, inject);
  }
  else
  {
    custom_min_adaptive_route(r, f, in_channel, outputs, inject);
  }
}

void custom_min_adaptive_local_route(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject)
{
  // Handle injection first
  if (inject)
  {
    outputs->Clear();
    outputs->AddRange(-1, 0, gNumVCs - 1);
    return;
  }

  // Calculate congestion threshold based on buffer occupancy
  int max_threshold = 0;
  for (int i = 0; i < 4; i++)
  {
    max_threshold += r->GetBufferOccupancy(i);
  }
  int congestion_threshold = max_threshold * gThresholdMultiplier;

  // Check congestion in both X and Y dimensions
  int east_cong = r->GetUsedCredit(0);  // East port
  int west_cong = r->GetUsedCredit(1);  // West port
  int south_cong = r->GetUsedCredit(2); // South port
  int north_cong = r->GetUsedCredit(3); // North port

  // Calculate average congestion
  int avg_congestion = (east_cong + west_cong + south_cong + north_cong) / 4;

  if (avg_congestion < congestion_threshold)
  {
    dim_order_route(r, f, in_channel, outputs, inject);
  }
  else
  {
    custom_min_adaptive_route(r, f, in_channel, outputs, inject);
  }
}

void custom_global_route(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject)
{
  if (inject)
  {
    outputs->Clear();
    outputs->AddRange(-1, 0, gNumVCs - 1);
    return;
  }

  int global_max_threshold = 0;
  for (int i = 0; i < 2 * gN; i++) // Check all input/output ports
  {
    global_max_threshold += r->GetBufferOccupancy(i);
  }
  int global_congestion_threshold = global_max_threshold * gThresholdMultiplier / gN; // Average across all ports

  int east_cong = r->GetUsedCredit(0);  // East port
  int west_cong = r->GetUsedCredit(1);  // West port
  int south_cong = r->GetUsedCredit(2); // South port
  int north_cong = r->GetUsedCredit(3); // North port

  int global_avg_congestion = (east_cong + west_cong + south_cong + north_cong) / 4;

  if (global_avg_congestion < global_congestion_threshold)
  {
    dim_order_route(r, f, in_channel, outputs, inject);
  }
  else
  {
    odd_even_route(r, f, in_channel, outputs, inject);
  }
}

void custom_route(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject)
{
  // Handle injection first
  if (inject)
  {
    outputs->Clear();
    outputs->AddRange(-1, 0, gNumVCs - 1);
    return;
  }

  // Calculate congestion threshold based on buffer occupancy
  int max_threshold = 0;
  for (int i = 0; i < 4; i++)
  {
    max_threshold += r->GetBufferOccupancy(i);
  }
  int congestion_threshold = max_threshold * gThresholdMultiplier;

  // Check congestion in both X and Y dimensions
  int east_cong = r->GetUsedCredit(0);  // East port
  int west_cong = r->GetUsedCredit(1);  // West port
  int south_cong = r->GetUsedCredit(2); // South port
  int north_cong = r->GetUsedCredit(3); // North port

  // Calculate average congestion
  int avg_congestion = (east_cong + west_cong + south_cong + north_cong) / 4;

  if (avg_congestion < congestion_threshold)
  {
    dim_order_route(r, f, in_channel, outputs, inject);
  }
  else
  {
    odd_even_route(r, f, in_channel, outputs, inject);
  }
}

void odd_even_route(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject)
{
  // Handle injection first
  if (inject)
  {
    outputs->Clear();
    outputs->AddRange(-1, 0, gNumVCs - 1);
    return;
  }

  // Get current and destination coordinates
  int cur = r->GetID();
  int dest = f->dest;

  // Calculate current and destination coordinates
  int cur_x = cur % gK;
  int cur_y = cur / gK;
  int dest_x = dest % gK;
  int dest_y = dest / gK;

  outputs->Clear();

  // Assign VCs based on direction to prevent deadlock
  // Use first half of VCs for X dimension, second half for Y dimension
  int const available_vcs = gNumVCs / 2;
  assert(available_vcs > 0);

  int vcBegin = 0;
  int vcEnd = gNumVCs - 1;

  // If we've reached the destination column, only use Y-dimension VCs
  if (cur_x == dest_x)
  {
    vcBegin = available_vcs; // Use second half of VCs for Y movement
    if (cur_y > dest_y)
    {
      outputs->AddRange(3, vcBegin, vcEnd); // North
    }
    else if (cur_y < dest_y)
    {
      outputs->AddRange(2, vcBegin, vcEnd); // South
    }
    else
    {
      outputs->AddRange(2 * gN, vcBegin, vcEnd); // Eject
    }
    return;
  }

  // Determine available turns based on odd-even rules
  bool is_x_even = (cur_x % 2 == 0);
  vector<int> available_ports;

  // Moving East
  if (cur_x < dest_x)
  {
    available_ports.push_back(0); // East port
    vcEnd = available_vcs - 1;    // Use first half of VCs for X movement

    // At even x-coordinates, can't turn north/south after going east
    if (!is_x_even && cur_y != dest_y)
    {
      vcBegin = available_vcs; // Use second half of VCs for Y movement
      vcEnd = gNumVCs - 1;
      if (cur_y < dest_y)
      {
        available_ports.push_back(2); // South
      }
      if (cur_y > dest_y)
      {
        available_ports.push_back(3); // North
      }
    }
  }
  // Moving West
  else if (cur_x > dest_x)
  {
    available_ports.push_back(1); // West port
    vcEnd = available_vcs - 1;    // Use first half of VCs for X movement

    // At odd x-coordinates, can't turn from north/south to west
    if (is_x_even && cur_y != dest_y)
    {
      vcBegin = available_vcs; // Use second half of VCs for Y movement
      vcEnd = gNumVCs - 1;
      if (cur_y < dest_y)
      {
        available_ports.push_back(2); // South
      }
      if (cur_y > dest_y)
      {
        available_ports.push_back(3); // North
      }
    }
  }

  // Add ports with proper VC ranges
  for (size_t i = 0; i < available_ports.size(); ++i)
  {
    int port = available_ports[i];
    // Use congestion to determine order of ports
    int congestion = r->GetUsedCredit(port);
    if (congestion < r->GetUsedCredit(available_ports[0]))
    {
      outputs->Clear(); // Clear previous ports if we found a less congested one
    }
    outputs->AddRange(port, vcBegin, vcEnd);
  }

  // If no ports were added (shouldn't happen), use dimension ordered routing as fallback
  if (outputs->GetSet().empty())
  {
    dim_order_route(r, f, in_channel, outputs, inject);
  }
}

int dim_order_helper(int cur, int dest, bool descending = false);

void dim_order_route(const Router *r, const Flit *f, int in_channel, OutputSet *outputs, bool inject)
{
  int out_port = inject ? -1 : dim_order_helper(r->GetID(), f->dest);

  int vcBegin = 0, vcEnd = gNumVCs - 1;
  if (f->type == Flit::READ_REQUEST)
  {
    vcBegin = gReadReqBeginVC;
    vcEnd = gReadReqEndVC;
  }
  else if (f->type == Flit::WRITE_REQUEST)
  {
    vcBegin = gWriteReqBeginVC;
    vcEnd = gWriteReqEndVC;
  }
  else if (f->type == Flit::READ_REPLY)
  {
    vcBegin = gReadReplyBeginVC;
    vcEnd = gReadReplyEndVC;
  }
  else if (f->type == Flit::WRITE_REPLY)
  {
    vcBegin = gWriteReplyBeginVC;
    vcEnd = gWriteReplyEndVC;
  }
  assert(((f->vc >= vcBegin) && (f->vc <= vcEnd)) || (inject && (f->vc < 0)));

  if (!inject && f->watch)
  {
    *gWatchOut << GetSimTime() << " | " << r->FullName() << " | "
               << "Adding VC range ["
               << vcBegin << ","
               << vcEnd << "]"
               << " at output port " << out_port
               << " for flit " << f->id
               << " (input port " << in_channel
               << ", destination " << f->dest << ")"
               << "." << endl;
  }

  outputs->Clear();

  outputs->AddRange(out_port, vcBegin, vcEnd);
}

int dim_order_helper(int cur, int dest, bool descending)
{
  if (cur == dest)
  {
    return 2 * gN; // Eject
  }

  int dim_left;

  if (descending)
  {
    for (dim_left = (gN - 1); dim_left > 0; --dim_left)
    {
      if ((cur * gK / gNodes) != (dest * gK / gNodes))
      {
        break;
      }
      cur = (cur * gK) % gNodes;
      dest = (dest * gK) % gNodes;
    }
    cur = (cur * gK) / gNodes;
    dest = (dest * gK) / gNodes;
  }
  else
  {
    for (dim_left = 0; dim_left < (gN - 1); ++dim_left)
    {
      if ((cur % gK) != (dest % gK))
      {
        break;
      }
      cur /= gK;
      dest /= gK;
    }
    cur %= gK;
    dest %= gK;
  }

  if (cur < dest)
  {
    return 2 * dim_left; // Right
  }
  else
  {
    return 2 * dim_left + 1; // Left
  }
}
