#!/usr/bin/env python

from __future__ import print_function
import os, sys, optparse
import ROOT

cols = [ROOT.kBlue, ROOT.kRed, ROOT.kGreen]

def parseArgs():
  parser= optparse.OptionParser(usage="%prog [OPTIONS] TIMING.CSV")
  parser.add_option ("-v", "--verbose",      help="verbose running", action="count", default=0)
  parser.add_option ("-i", "--interactive",  help="ROOT interactive mode", action="store_true")
  parser.add_option ("-o", "--output",       help="plot pdf file")
  parser.add_option ("-l", "--legends",      help="comma-separated list of legends")
  opt, args= parser.parse_args()
  if not args:
    parser.print_help()
    exit(1)
  if opt.output is None:
    r,e = os.path.splitext(args[0])
    opt.output = r + ".pdf"
  return opt, args

def process ():
  opt, args = parseArgs()
  if not opt.interactive: ROOT.gROOT.SetBatch(True)
  ROOT.TH1.AddDirectory(0)
  legends = opt.legends.split(",") if opt.legends else []
  trees = []
  for i,fspec in enumerate(reversed(args)):
    name = "times_%d" % (len(args)-i-1)
    tree = ROOT.TTree(name,name)
    trees.insert(0,tree)
    for i,f in enumerate(fspec.split(",")):
      if not i:
        n = tree.ReadFile(f,"",",")
      else:
        # skip header line
        fs = ROOT.std.ifstream(f)
        while chr(fs.get()) != '\n': pass
        n = tree.ReadStream(fs,"",",")
        del fs
      if not n: return 1
  nt = len(trees)
  sf = 1.0 / float(nt)
  w = min (0.45, 0.55*sf)
  ROOT.gStyle.SetPaintTextFormat(".0f")
  ROOT.gStyle.SetTickLength(0.0)
  tree0 = trees[0]
  tree0.Draw("label","","goff")
  h0 = tree.GetHistogram()
  h0.SetStats(0)
  h0.SetTitle(";;ns / double")
  ax = h0.GetXaxis()
  hists = []
  for i,tree in enumerate(trees):
    h = h0.Clone("hist_%d" % i)
    hists.append(h)
    h.Reset("M")
    h.SetMinimum(0.0)
    h.SetBarWidth(w)
    h.SetBarOffset(0.3 + 1.3*w*i)
    h.SetFillColor(cols[i%len(cols)])
    h.SetMarkerSize (min (1.0, 3.5*w))
    hn = h.Clone("nent_%d" % i)
    for e in tree:
      nval = e.entries * e.branches * e.elements
      bin = ax.FindBin(e.label)
      print ("%s:%d %s %s %-20s %s %-10s %1s %d %d %d %5g %5g (%d)" %
             (tree.GetName(), bin, e.time, e.host, e.label, e.testcase, e.test, e.fill, e.entries, e.branches, e.elements, e.ms, e.cpu, nval))
      if bin < 0:
        print("no bin",e.label)
        continue
      h.AddBinContent (bin, 1000000.0 * e.ms / float(nval))
      hn.AddBinContent (bin)
    h.Divide(hn)
    del hn
  if legends:
    leg = ROOT.TLegend (0.8,0.85,0.9,0.9)
    leg.SetFillStyle(0)
  for i,h in enumerate(hists):
    if not i: h.Draw("bar text0")
    else:     h.Draw("bar text0 same")
    if legends and i<len(legends): leg.AddEntry(h,legends[i],"f")
  if legends: leg.Draw()
  if opt.interactive: waitPlot()
  else: ROOT.gPad.Print(opt.output)


def waitPlot():
  if not ROOT.gPad: return
  canvas= ROOT.gPad.GetCanvas()
  if not canvas: return
  print ("Double-click or press any key in window '"+canvas.GetTitle()+"' to continue...")
  canvas.WaitPrimitive()

exit (process())
